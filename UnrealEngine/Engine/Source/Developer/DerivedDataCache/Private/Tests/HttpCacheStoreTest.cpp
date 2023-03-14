// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/ParallelFor.h"
#include "DerivedDataBackendInterface.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataValue.h"
#include "DerivedDataValueId.h"
#include "HAL/FileManager.h"
#include "Memory/CompositeBuffer.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/CompactBinaryWriter.h"
#include "ZenServerInterface.h"

// Test is targeted at HttpDerivedDataBackend but with some backend test interface it could be generalized
// to function against all backends.

#if WITH_DEV_AUTOMATION_TESTS && WITH_HTTP_DDC_BACKEND

DEFINE_LOG_CATEGORY_STATIC(LogHttpDerivedDataBackendTests, Log, All);

#define TEST_NAME_ROOT "System.DerivedDataCache.HttpDerivedDataBackend"

#define IMPLEMENT_HTTPDERIVEDDATA_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
	IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(TClass, FHttpCacheStoreTestBase, TEXT(TEST_NAME_ROOT PrettyName), TFlags) \
	void TClass::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const \
	{ \
		if (CheckPrequisites()) \
		{ \
			OutBeautifiedNames.Add(TEXT(TEST_NAME_ROOT PrettyName)); \
			OutTestCommands.Add(FString()); \
		} \
	}

namespace UE::DerivedData
{

ILegacyCacheStore* GetAnyHttpCacheStore(
	FString& OutDomain,
	FString& OutOAuthProvider,
	FString& OutOAuthClientId,
	FString& OutOAuthSecret,
	FString& OutOAuthScope,
	FString& OAuthProviderIdentifier,
	FString& OAuthAccessToken,
	FString& OutNamespace);

TTuple<ILegacyCacheStore*, ECacheStoreFlags> CreateZenCacheStore(const TCHAR* NodeName, const TCHAR* Config);

class FHttpCacheStoreTestBase : public FAutomationTestBase
{
public:
	FHttpCacheStoreTestBase(const FString& InName, const bool bInComplexTask)
	: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	bool CheckPrequisites() const
	{
		if (ILegacyCacheStore* Backend = GetTestBackend())
		{
			return true;
		}
		return false;
	}

protected:

	void ConcurrentTestWithStats(TFunctionRef<void()> TestFunction, int32 ThreadCount, double Duration)
	{
		std::atomic<uint64> Requests{ 0 };
		std::atomic<uint64> MaxLatency{ 0 };
		std::atomic<uint64> TotalCycles{ 0 };
		std::atomic<uint64> TotalRequests{ 0 };

		FEvent* StartEvent = FPlatformProcess::GetSynchEventFromPool(true);
		FEvent* LastEvent = FPlatformProcess::GetSynchEventFromPool(true);
		std::atomic<double> StopTime{ 0.0 };
		std::atomic<uint64> ActiveCount{ 0 };

		for (int32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
		{
			ActiveCount++;
			Async(
				ThreadIndex < FTaskGraphInterface::Get().GetNumWorkerThreads() ? EAsyncExecution::TaskGraph : EAsyncExecution::Thread,
				[&]()
				{
					// No false start, wait until everyone is ready before starting the test
					StartEvent->Wait();

					while (FPlatformTime::Seconds() < StopTime.load(std::memory_order_relaxed))
					{
						const uint64 Before = FPlatformTime::Cycles64();
						TestFunction();
						const uint64 Delta = FPlatformTime::Cycles64() - Before;
						Requests++;
						TotalCycles += Delta;
						TotalRequests++;

						// Compare exchange loop until we either succeed to set the maximum value
						// or we bail out because we don't have the maximum value anymore.
						while (true)
						{
							uint64 Snapshot = MaxLatency.load();
							if (Delta > Snapshot)
							{
								// Only do the exchange if the value has not changed since we confirmed
								// we had a bigger one.
								if (MaxLatency.compare_exchange_strong(Snapshot, Delta))
								{
									// Exchange succeeded
									break;
								}
							}
							else
							{
								// We don't have the maximum
								break;
							}
						}
					}
					
					if (--ActiveCount == 0)
					{
						LastEvent->Trigger();
					}
				}
			);
		}

		StopTime = FPlatformTime::Seconds() + Duration;

		// GO!
		StartEvent->Trigger();

		while (FPlatformTime::Seconds() < StopTime)
		{
			FPlatformProcess::Sleep(1.0f);

			if (TotalRequests)
			{
				UE_LOG(LogHttpDerivedDataBackendTests, Display, TEXT("RPS: %" UINT64_FMT ", AvgLatency: %.02f ms, MaxLatency: %.02f s"),
					Requests.exchange(0), FPlatformTime::ToMilliseconds64(TotalCycles) / double(TotalRequests), FPlatformTime::ToSeconds64(MaxLatency));
			}
			else
			{
				UE_LOG(LogHttpDerivedDataBackendTests, Display, TEXT("RPS: %" UINT64_FMT ", AvgLatency: N/A, MaxLatency: %.02f s"),
					Requests.exchange(0), FPlatformTime::ToSeconds64(MaxLatency));
			}
		}

		LastEvent->Wait();

		FPlatformProcess::ReturnSynchEventToPool(StartEvent);
		FPlatformProcess::ReturnSynchEventToPool(LastEvent);
	}

	static ILegacyCacheStore* GetTestBackend()
	{
		static ILegacyCacheStore* CachedBackend = GetAnyHttpCacheStore(
			TestDomain, TestOAuthProvider, TestOAuthClientId, TestOAuthSecret, TestOAuthScope, TestOAuthProviderIdentifier, TestOAuthAccessToken, TestNamespace);
		return CachedBackend;
	}

	bool GetRecords(TConstArrayView<FCacheRecord> Records, FCacheRecordPolicy Policy, TArray<FCacheRecord>& OutRecords)
	{
		using namespace UE::DerivedData;
		ICacheStore* TestBackend = GetTestBackend();

		TArray<FCacheGetRequest> Requests;
		Requests.Reserve(Records.Num());

		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			const FCacheRecord& Record = Records[RecordIndex];
			Requests.Add({ {TEXT("FHttpCacheStoreTestBase")}, Record.GetKey(), Policy, static_cast<uint64>(RecordIndex) });
		}

		struct FGetOutput
		{
			FCacheRecord Record;
			EStatus Status = EStatus::Error;
		};

		TArray<TOptional<FGetOutput>> GetOutputs;
		GetOutputs.SetNum(Records.Num());
		FRequestOwner RequestOwner(EPriority::Blocking);
		TestBackend->Get(Requests, RequestOwner, [&GetOutputs](FCacheGetResponse&& Response)
			{
				FCacheRecordBuilder RecordBuilder(Response.Record.GetKey());

				if (Response.Record.GetMeta())
				{
					RecordBuilder.SetMeta(FCbObject::Clone(Response.Record.GetMeta()));
				}

				for (const FValueWithId& Value : Response.Record.GetValues())
				{
					if (Value)
					{
						RecordBuilder.AddValue(Value);
					}
				}
				GetOutputs[int32(Response.UserData)].Emplace(FGetOutput{ RecordBuilder.Build(), Response.Status });
			});
		RequestOwner.Wait();

		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			FGetOutput& ReceivedOutput = GetOutputs[RecordIndex].GetValue();
			if (ReceivedOutput.Status != EStatus::Ok)
			{
				return false;
			}
			OutRecords.Add(MoveTemp(ReceivedOutput.Record));
		}

		return true;
	}

	bool GetValues(TConstArrayView<FValue> Values, ECachePolicy Policy, TArray<FValue>& OutValues, const char* BucketName = nullptr, ICacheStore* CacheStore = GetTestBackend())
	{
		using namespace UE::DerivedData;
		ICacheStore* TestBackend = CacheStore;
		FCacheBucket TestCacheBucket(BucketName ? BucketName : "AutoTestDummy");

		TArray<FCacheGetValueRequest> Requests;
		Requests.Reserve(Values.Num());

		for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
		{
			const FValue& Value = Values[ValueIndex];
			FCacheKey Key;
			Key.Bucket = TestCacheBucket;
			Key.Hash = Value.GetRawHash();
			Requests.Add({ {TEXT("FHttpCacheStoreTestBase")}, Key, Policy, static_cast<uint64>(ValueIndex) });
		}

		struct FGetValueOutput
		{
			FValue Value;
			EStatus Status = EStatus::Error;
		};

		TArray<TOptional<FGetValueOutput>> GetValueOutputs;
		GetValueOutputs.SetNum(Values.Num());
		FRequestOwner RequestOwner(EPriority::Blocking);
		TestBackend->GetValue(Requests, RequestOwner, [&GetValueOutputs](FCacheGetValueResponse&& Response)
			{
				GetValueOutputs[int32(Response.UserData)].Emplace(FGetValueOutput{ Response.Value, Response.Status });
			});
		RequestOwner.Wait();

		for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
		{
			FGetValueOutput& ReceivedOutput = GetValueOutputs[ValueIndex].GetValue();
			if (ReceivedOutput.Status != EStatus::Ok)
			{
				return false;
			}
			OutValues.Add(MoveTemp(ReceivedOutput.Value));
		}

		return true;
	}

	bool GetRecordChunks(TConstArrayView<FCacheRecord> Records, FCacheRecordPolicy Policy, uint64 Offset, uint64 Size, TArray<FSharedBuffer>& OutChunks)
	{
		using namespace UE::DerivedData;
		ICacheStore* TestBackend = GetTestBackend();

		TArray<FCacheGetChunkRequest> Requests;

		int32 OverallIndex = 0;
		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			const FCacheRecord& Record = Records[RecordIndex];
			TConstArrayView<FValueWithId> Values = Record.GetValues();
			for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
			{
				const FValueWithId& Value = Values[ValueIndex];
				Requests.Add({ {TEXT("FHttpCacheStoreTestBase")}, Record.GetKey(),  Value.GetId(), Offset, Size, Value.GetRawHash(), Policy.GetValuePolicy(Value.GetId()), static_cast<uint64>(OverallIndex) });
				++OverallIndex;
			}
		}

		struct FGetChunksOutput
		{
			FSharedBuffer Chunk;
			EStatus Status = EStatus::Error;
		};

		TArray<TOptional<FGetChunksOutput>> GetOutputs;
		GetOutputs.SetNum(Requests.Num());
		FRequestOwner RequestOwner(EPriority::Blocking);
		TestBackend->GetChunks(Requests, RequestOwner, [&GetOutputs](FCacheGetChunkResponse&& Response)
			{
				GetOutputs[int32(Response.UserData)].Emplace(FGetChunksOutput { Response.RawData, Response.Status });
			});
		RequestOwner.Wait();

		for (int32 RequestIndex = 0; RequestIndex < Requests.Num(); ++RequestIndex)
		{
			FGetChunksOutput& ReceivedOutput = GetOutputs[RequestIndex].GetValue();
			if (ReceivedOutput.Status != EStatus::Ok)
			{
				return false;
			}
			OutChunks.Add(MoveTemp(ReceivedOutput.Chunk));
		}

		return true;

	}
	
	void ValidateRecords(const TCHAR* Name, TConstArrayView<FCacheRecord> RecordsToTest, TConstArrayView<FCacheRecord> ReferenceRecords, FCacheRecordPolicy Policy)
	{
		using namespace UE::DerivedData;

		if (!TestEqual(FString::Printf(TEXT("%s::Record quantity"), Name), RecordsToTest.Num(), ReferenceRecords.Num()))
		{
			return;
		}

		for (int32 RecordIndex = 0; RecordIndex < RecordsToTest.Num(); ++RecordIndex)
		{
			const FCacheRecord& ExpectedRecord = ReferenceRecords[RecordIndex];
			const FCacheRecord& RecordToTest = RecordsToTest[RecordIndex];

			if (EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::SkipMeta))
			{
				TestTrue(FString::Printf(TEXT("%s::Get meta null"), Name), !RecordToTest.GetMeta());
			}
			else
			{
				TestTrue(FString::Printf(TEXT("%s::Get meta equality"), Name), ExpectedRecord.GetMeta().Equals(RecordToTest.GetMeta()));
			}

			TestEqual(FString::Printf(TEXT("%s::Get value quantity"), Name), ExpectedRecord.GetValues().Num(), RecordToTest.GetValues().Num());

			const TConstArrayView<FValueWithId> ExpectedValues = ExpectedRecord.GetValues();
			const TConstArrayView<FValueWithId> ReceivedValues = RecordToTest.GetValues();
			for (int32 ValueIndex = 0; ValueIndex < ExpectedValues.Num(); ++ValueIndex)
			{
				if (EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::SkipData))
				{
					TestTrue(FString::Printf(TEXT("%s::Get value[%d] !HasData"), Name, ValueIndex), !ReceivedValues[ValueIndex].HasData());
				}
				else
				{
					TestTrue(FString::Printf(TEXT("%s::Get value[%d] HasData"), Name, ValueIndex), ReceivedValues[ValueIndex].HasData());
					TestTrue(FString::Printf(TEXT("%s::Get value[%d] equality"), Name, ValueIndex), ExpectedValues[ValueIndex] == ReceivedValues[ValueIndex]);
					TestTrue(FString::Printf(TEXT("%s::Get value[%d] data equality"), Name, ValueIndex), FIoHash::HashBuffer(ReceivedValues[ValueIndex].GetData().GetCompressed()) == FIoHash::HashBuffer(ExpectedValues[ValueIndex].GetData().GetCompressed()));
				}
			}
		}
	}

	void ValidateValues(const TCHAR* Name, TConstArrayView<FValue> ValuesToTest, TConstArrayView<FValue> ReferenceValues, ECachePolicy Policy)
	{
		using namespace UE::DerivedData;

		if (!TestEqual(FString::Printf(TEXT("%s::Value quantity"), Name), ValuesToTest.Num(), ReferenceValues.Num()))
		{
			return;
		}

		for (int32 ValueIndex = 0; ValueIndex < ValuesToTest.Num(); ++ValueIndex)
		{
			const FValue& ExpectedValue = ReferenceValues[ValueIndex];
			const FValue& ValueToTest = ValuesToTest[ValueIndex];

			if (EnumHasAnyFlags(Policy, ECachePolicy::SkipData))
			{
				TestTrue(FString::Printf(TEXT("%s::Get value[%d] !HasData"), Name, ValueIndex), !ValueToTest.HasData());
			}
			else
			{
				TestTrue(FString::Printf(TEXT("%s::Get value[%d] HasData"), Name, ValueIndex), ValueToTest.HasData());
				TestTrue(FString::Printf(TEXT("%s::Get value[%d] equality"), Name, ValueIndex), ExpectedValue == ValueToTest);
				TestTrue(FString::Printf(TEXT("%s::Get value[%d] data equality"), Name, ValueIndex), FIoHash::HashBuffer(ValueToTest.GetData().GetCompressed()) == FIoHash::HashBuffer(ExpectedValue.GetData().GetCompressed()));
			}
		}
	}

	void ValidateRecordChunks(const TCHAR* Name, TConstArrayView<FSharedBuffer> RecordChunksToTest, TConstArrayView<FCacheRecord> ReferenceRecords, FCacheRecordPolicy Policy, uint64 Offset, uint64 Size)
	{
		using namespace UE::DerivedData;

		int32 TotalChunks = 0;
		for (int32 RecordIndex = 0; RecordIndex < ReferenceRecords.Num(); ++RecordIndex)
		{
			const FCacheRecord& Record = ReferenceRecords[RecordIndex];
			TConstArrayView<FValueWithId> Values = Record.GetValues();
			for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
			{
				++TotalChunks;
			}
		}

		if (!TestEqual(FString::Printf(TEXT("%s::Chunk quantity"), Name), RecordChunksToTest.Num(), TotalChunks))
		{
			return;
		}

		int32 ChunkIndex = 0;
		for (int32 RecordIndex = 0; RecordIndex < ReferenceRecords.Num(); ++RecordIndex)
		{
			const FCacheRecord& ExpectedRecord = ReferenceRecords[RecordIndex];

			const TConstArrayView<FValueWithId> ExpectedValues = ExpectedRecord.GetValues();
			for (int32 ValueIndex = 0; ValueIndex < ExpectedValues.Num(); ++ValueIndex)
			{
				const FSharedBuffer& ChunkToTest = RecordChunksToTest[ChunkIndex];

				if (EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::SkipData))
				{
					TestTrue(FString::Printf(TEXT("%s::Get chunk[%d] IsNull"), Name, ChunkIndex), ChunkToTest.IsNull());
				}
				else
				{
					FSharedBuffer ReferenceBuffer = ExpectedValues[ValueIndex].GetData().Decompress();
					FMemoryView ReferenceView = ReferenceBuffer.GetView().Mid(Offset, Size);
					TestTrue(FString::Printf(TEXT("%s::Get chunk[%d] data equality"), Name, ChunkIndex), ReferenceView.EqualBytes(ChunkToTest.GetView()));
				}
				++ChunkIndex;
			}
		}
	}

	TArray<FCacheRecord> GetAndValidateRecords(const TCHAR* Name, TConstArrayView<FCacheRecord> Records, FCacheRecordPolicy Policy)
	{
		using namespace UE::DerivedData;
		TArray<FCacheRecord> ReceivedRecords;
		bool bGetSuccessful = GetRecords(Records, Policy, ReceivedRecords);
		TestTrue(FString::Printf(TEXT("%s::Get status"), Name), bGetSuccessful);

		if (!bGetSuccessful)
		{
			return TArray<FCacheRecord>();
		}

		ValidateRecords(Name, ReceivedRecords, Records, Policy);
		return ReceivedRecords;
	}

	TArray<FValue> GetAndValidateValues(const TCHAR* Name, TConstArrayView<FValue> Values, ECachePolicy Policy, ICacheStore* CacheStore = GetTestBackend())
	{
		using namespace UE::DerivedData;
		TArray<FValue> ReceivedValues;
		bool bGetSuccessful = GetValues(Values, Policy, ReceivedValues, nullptr, CacheStore);
		TestTrue(FString::Printf(TEXT("%s::Get status"), Name), bGetSuccessful);

		if (!bGetSuccessful)
		{
			return TArray<FValue>();
		}

		ValidateValues(Name, ReceivedValues, Values, Policy);
		return ReceivedValues;
	}

	TArray<FSharedBuffer> GetAndValidateRecordChunks(const TCHAR* Name, TConstArrayView<FCacheRecord> Records, FCacheRecordPolicy Policy, uint64 Offset, uint64 Size)
	{
		using namespace UE::DerivedData;
		TArray<FSharedBuffer> ReceivedChunks;
		bool bGetSuccessful = GetRecordChunks(Records, Policy, Offset, Size, ReceivedChunks);
		TestTrue(FString::Printf(TEXT("%s::GetChunks status"), Name), bGetSuccessful);

		if (!bGetSuccessful)
		{
			return TArray<FSharedBuffer>();
		}

		ValidateRecordChunks(Name, ReceivedChunks, Records, Policy, Offset, Size);
		return ReceivedChunks;
	}

	TArray<FCacheRecord> GetAndValidateRecordsAndChunks(const TCHAR* Name, TConstArrayView<FCacheRecord> Records, FCacheRecordPolicy Policy)
	{
		GetAndValidateRecordChunks(Name, Records, Policy, 5, 5);
		return GetAndValidateRecords(Name, Records, Policy);
	}

protected:
	static inline FString TestDomain;
	static inline FString TestOAuthProvider;
	static inline FString TestOAuthClientId;
	static inline FString TestOAuthSecret;
	static inline FString TestOAuthScope;
	static inline FString TestOAuthProviderIdentifier;
	static inline FString TestOAuthAccessToken;
	static inline FString TestNamespace;
};

TArray<FCacheRecord> CreateTestCacheRecords(ICacheStore* InTestBackend, uint32 InNumKeys, uint32 InNumValues, FCbObject MetaContents = FCbObject(), const char* BucketName = nullptr)
{
	using namespace UE::DerivedData;
	FCacheBucket TestCacheBucket(BucketName ? BucketName : "AutoTestDummy");

	TArray<FCacheRecord> CacheRecords;
	TArray<FCachePutRequest> PutRequests;
	PutRequests.Reserve(InNumKeys);

	for (uint32 KeyIndex = 0; KeyIndex < InNumKeys; ++KeyIndex)
	{
		FIoHashBuilder HashBuilder;

		TArray<FSharedBuffer> Values;
		for (uint32 ValueIndex = 0; ValueIndex < InNumValues; ++ValueIndex)
		{
			TArray<uint8> ValueContents;
			// Add N zeroed bytes where N corresponds to the value index times 10.
			const int32 NumBytes = (ValueIndex+1)*10;
			ValueContents.AddUninitialized(NumBytes);
			for (int32 ContentIndex = 0; ContentIndex < NumBytes; ++ContentIndex)
			{
				ValueContents[ContentIndex] = (uint8)(KeyIndex + ContentIndex);
			}
			Values.Emplace(MakeSharedBufferFromArray(MoveTemp(ValueContents)));
			HashBuilder.Update(Values.Last().GetView());
		}

		FCacheKey Key;
		Key.Bucket = TestCacheBucket;
		Key.Hash = HashBuilder.Finalize();

		FCacheRecordBuilder RecordBuilder(Key);

		for (const FSharedBuffer& ValueBuffer : Values)
		{
			FIoHash ValueHash(FIoHash::HashBuffer(ValueBuffer));
			RecordBuilder.AddValue(FValueId::FromHash(ValueHash), ValueBuffer);
		}

		if (MetaContents)
		{
			RecordBuilder.SetMeta(MoveTemp(MetaContents));
		}

		PutRequests.Add({ {TEXT("AutoTest")}, RecordBuilder.Build(), ECachePolicy::Default, KeyIndex });
	}

	FRequestOwner Owner(EPriority::Blocking);
	InTestBackend->Put(PutRequests, Owner, [&CacheRecords, &PutRequests] (FCachePutResponse&& Response)
	{
		check(Response.Status == EStatus::Ok);
	});
	Owner.Wait();

	CacheRecords.Reserve(PutRequests.Num());
	for (const FCachePutRequest& PutRequest : PutRequests)
	{
		CacheRecords.Add(PutRequest.Record);
	}

	return CacheRecords;
}

TArray<FValue> CreateTestCacheValues(ICacheStore* InTestBackend, uint32 InNumValues, const char* BucketName = nullptr)
{
	using namespace UE::DerivedData;
	FCacheBucket TestCacheBucket(BucketName ? BucketName : "AutoTestDummy");

	TArray<FCachePutValueRequest> PutValueRequests;
	PutValueRequests.Reserve(InNumValues);

	TArray<FSharedBuffer> ValueBuffers;
	for (uint32 ValueIndex = 0; ValueIndex < InNumValues; ++ValueIndex)
	{
		TArray<uint8> ValueContents;
		// Add N zeroed bytes where N corresponds to the value index times 10.
		const int32 NumBytes = (ValueIndex+1)*10;
		ValueContents.AddUninitialized(NumBytes);
		for (int32 ContentIndex = 0; ContentIndex < NumBytes; ++ContentIndex)
		{
			ValueContents[ContentIndex] = (uint8)(ValueIndex + ContentIndex + 52); // offset of 52 to keep the contents distinct from record test data
		}
		ValueBuffers.Emplace(MakeSharedBufferFromArray(MoveTemp(ValueContents)));
	}

	uint64 KeyIndex = 0;
	for (const FSharedBuffer& ValueBuffer : ValueBuffers)
	{
		FIoHash ValueHash(FIoHash::HashBuffer(ValueBuffer));

		FCacheKey Key;
		Key.Bucket = TestCacheBucket;
		Key.Hash = ValueHash;

		PutValueRequests.Add({ {TEXT("AutoTest")}, Key, FValue::Compress(ValueBuffer), ECachePolicy::Default, KeyIndex++ });
	}

	TArray<FValue> Values;
	FRequestOwner Owner(EPriority::Blocking);
	InTestBackend->PutValue(PutValueRequests, Owner, [&Values, &PutValueRequests](FCachePutValueResponse&& Response)
		{
			check(Response.Status == EStatus::Ok);
		});
	Owner.Wait();

	Values.Reserve(PutValueRequests.Num());
	for (const FCachePutValueRequest& PutValueRequest : PutValueRequests)
	{
		Values.Add(PutValueRequest.Value);
	}

	return Values;
}

// Tests basic functionality for structured cache operations
IMPLEMENT_HTTPDERIVEDDATA_AUTOMATION_TEST(FHttpCacheStoreTest, ".CacheStore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHttpCacheStoreTest::RunTest(const FString& Parameters)
{
	using namespace UE::DerivedData;
	ILegacyCacheStore* TestBackend = GetTestBackend();

#if UE_WITH_ZEN
	using namespace UE::Zen;
	FServiceSettings ZenUpstreamTestServiceSettings;
	FServiceAutoLaunchSettings& ZenUpstreamTestAutoLaunchSettings = ZenUpstreamTestServiceSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>();
	ZenUpstreamTestAutoLaunchSettings.DataPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineSavedDir(), "ZenUpstreamUnitTest"));
	ZenUpstreamTestAutoLaunchSettings.ExtraArgs = FString::Printf(TEXT("--http asio --upstream-jupiter-url \"%s\" --upstream-jupiter-oauth-url \"%s\" --upstream-jupiter-oauth-clientid \"%s\" --upstream-jupiter-oauth-clientsecret \"%s\" --upstream-jupiter-namespace \"%s\""),
		*TestDomain,
		*TestOAuthProvider,
		*TestOAuthClientId,
		*TestOAuthSecret,
		*TestNamespace
	);
	ZenUpstreamTestAutoLaunchSettings.DesiredPort = 23337; // Avoid the normal default port
	ZenUpstreamTestAutoLaunchSettings.bShowConsole = true;
	ZenUpstreamTestAutoLaunchSettings.bLimitProcessLifetime = true;
	FScopeZenService ScopeZenUpstreamService(MoveTemp(ZenUpstreamTestServiceSettings));

	IFileManager::Get().DeleteDirectory(*FPaths::Combine(FPaths::EngineSavedDir(), "ZenUpstreamSiblingUnitTest"), false, true);
	FServiceSettings ZenUpstreamSiblingTestServiceSettings;
	FServiceAutoLaunchSettings& ZenUpstreamSiblingTestAutoLaunchSettings = ZenUpstreamSiblingTestServiceSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>();
	ZenUpstreamSiblingTestAutoLaunchSettings.DataPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineSavedDir(), "ZenUpstreamSiblingUnitTest"));
	ZenUpstreamSiblingTestAutoLaunchSettings.ExtraArgs = FString::Printf(TEXT("--http asio --upstream-jupiter-url \"%s\" --upstream-jupiter-oauth-url \"%s\" --upstream-jupiter-oauth-clientid \"%s\" --upstream-jupiter-oauth-clientsecret \"%s\" --upstream-jupiter-namespace \"%s\""),
		*TestDomain,
		*TestOAuthProvider,
		*TestOAuthClientId,
		*TestOAuthSecret,
		*TestNamespace
	);
	ZenUpstreamSiblingTestAutoLaunchSettings.DesiredPort = 23338; // Avoid the normal default port
	ZenUpstreamSiblingTestAutoLaunchSettings.bShowConsole = true;
	ZenUpstreamSiblingTestAutoLaunchSettings.bLimitProcessLifetime = true;
	FScopeZenService ScopeZenUpstreamSiblingService(MoveTemp(ZenUpstreamSiblingTestServiceSettings));

	FServiceSettings ZenTestServiceSettings;
	FServiceAutoLaunchSettings& ZenTestAutoLaunchSettings = ZenTestServiceSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>();
	ZenTestAutoLaunchSettings.DataPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineSavedDir(), "ZenUnitTest"));
	ZenTestAutoLaunchSettings.ExtraArgs = FString::Printf(TEXT("--http asio --upstream-zen-url \"http://localhost:%d\""),
		ScopeZenUpstreamService.GetInstance().GetPort()
	);
	ZenTestAutoLaunchSettings.DesiredPort = 13337; // Avoid the normal default port
	ZenTestAutoLaunchSettings.bShowConsole = true;
	ZenTestAutoLaunchSettings.bLimitProcessLifetime = true;

	IFileManager::Get().DeleteDirectory(*FPaths::Combine(FPaths::EngineSavedDir(), "ZenUnitTestSibling"), false, true);
	FServiceSettings ZenTestServiceSiblingSettings;
	FServiceAutoLaunchSettings& ZenTestSiblingAutoLaunchSettings = ZenTestServiceSiblingSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>();
	ZenTestSiblingAutoLaunchSettings.DataPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineSavedDir(), "ZenUnitTestSibling"));
	ZenTestSiblingAutoLaunchSettings.ExtraArgs = FString::Printf(TEXT("--http asio --upstream-zen-url \"http://localhost:%d\""),
		ScopeZenUpstreamSiblingService.GetInstance().GetPort()
	);
	ZenTestSiblingAutoLaunchSettings.DesiredPort = 13338; // Avoid the normal default port
	ZenTestSiblingAutoLaunchSettings.bShowConsole = true;
	ZenTestSiblingAutoLaunchSettings.bLimitProcessLifetime = true;

	FScopeZenService ScopeZenSiblingService(MoveTemp(ZenTestServiceSiblingSettings));
	TUniquePtr<ILegacyCacheStore> ZenIntermediarySiblingBackend(CreateZenCacheStore(TEXT("TestSibling"),
		*FString::Printf(TEXT("Host=%s, StructuredNamespace=%s"), ScopeZenSiblingService.GetInstance().GetURL(), *TestNamespace)).Key);

	FScopeZenService ScopeZenService(MoveTemp(ZenTestServiceSettings));
	TUniquePtr<ILegacyCacheStore> ZenIntermediaryBackend(CreateZenCacheStore(TEXT("Test"),
		*FString::Printf(TEXT("Host=%s, StructuredNamespace=%s"), ScopeZenService.GetInstance().GetURL(), *TestNamespace)).Key);
	auto WaitForZenPushToUpstream = [](ILegacyCacheStore* ZenBackend, TConstArrayView<FCacheRecord> Records)
	{
		// TODO: Expecting a legitimate means to wait for zen to finish pushing records to its upstream in the future
		FPlatformProcess::Sleep(1.0f);
	};
	auto WaitForZenPushValuesToUpstream = [](ILegacyCacheStore* ZenBackend, TConstArrayView<FValue> Values)
	{
		// TODO: Expecting a legitimate means to wait for zen to finish pushing records to its upstream in the future
		FPlatformProcess::Sleep(1.0f);
	};
#endif // UE_WITH_ZEN

	const uint32 RecordsInBatch = 3;
	const uint32 ValuesInBatch = RecordsInBatch;

	{
		TArray<FCacheRecord> PutRecords = CreateTestCacheRecords(TestBackend, RecordsInBatch, 1);
		TArray<FCacheRecord> RecievedRecords = GetAndValidateRecordsAndChunks(TEXT("SimpleValue"), PutRecords, ECachePolicy::Default);
		TArray<FCacheRecord> RecievedRecordsSkipMeta = GetAndValidateRecordsAndChunks(TEXT("SimpleValueSkipMeta"), PutRecords, ECachePolicy::Default | ECachePolicy::SkipMeta);
		TArray<FCacheRecord> RecievedRecordsSkipData = GetAndValidateRecordsAndChunks(TEXT("SimpleValueSkipData"), PutRecords, ECachePolicy::Default | ECachePolicy::SkipData);

#if UE_WITH_ZEN
		if (ZenIntermediaryBackend)
		{
			TArray<FCacheRecord> PutRecordsZen = CreateTestCacheRecords(ZenIntermediaryBackend.Get(), RecordsInBatch, 1, FCbObject(), "AutoTestDummyZen");
			WaitForZenPushToUpstream(ZenIntermediaryBackend.Get(), PutRecordsZen);
			ValidateRecords(TEXT("SimpleValueZenAndDirect"), GetAndValidateRecords(TEXT("SimpleValueZen"), PutRecordsZen, ECachePolicy::Default), RecievedRecords, ECachePolicy::Default);
			ValidateRecords(TEXT("SimpleValueSkipMetaZenAndDirect"), GetAndValidateRecords(TEXT("SimpleValueSkipMetaZen"), PutRecordsZen, ECachePolicy::Default | ECachePolicy::SkipMeta), RecievedRecordsSkipMeta, ECachePolicy::Default | ECachePolicy::SkipMeta);
			ValidateRecords(TEXT("SimpleValueSkipDataZenAndDirect"), GetAndValidateRecords(TEXT("SimpleValueSkipDataZen"), PutRecordsZen, ECachePolicy::Default | ECachePolicy::SkipData), RecievedRecordsSkipData, ECachePolicy::Default | ECachePolicy::SkipData);
		}
#endif // UE_WITH_ZEN
	}

	{
		TCbWriter<64> MetaWriter;
		MetaWriter.BeginObject();
		MetaWriter.AddInteger(ANSITEXTVIEW("MetaKey"), 42);
		MetaWriter.EndObject();
		FCbObject MetaObject = MetaWriter.Save().AsObject();

		TArray<FCacheRecord> PutRecords = CreateTestCacheRecords(TestBackend, RecordsInBatch, 1, MetaObject);
		TArray<FCacheRecord> RecievedRecords = GetAndValidateRecordsAndChunks(TEXT("SimpleValueWithMeta"), PutRecords, ECachePolicy::Default);
		TArray<FCacheRecord> RecievedRecordsSkipMeta = GetAndValidateRecordsAndChunks(TEXT("SimpleValueWithMetaSkipMeta"), PutRecords, ECachePolicy::Default | ECachePolicy::SkipMeta);
		TArray<FCacheRecord> RecievedRecordsSkipData = GetAndValidateRecordsAndChunks(TEXT("SimpleValueWithMetaSkipData"), PutRecords, ECachePolicy::Default | ECachePolicy::SkipData);

#if UE_WITH_ZEN
		if (ZenIntermediaryBackend)
		{
			TArray<FCacheRecord> PutRecordsZen = CreateTestCacheRecords(ZenIntermediaryBackend.Get(), RecordsInBatch, 1, MetaObject, "AutoTestDummyZen");
			WaitForZenPushToUpstream(ZenIntermediaryBackend.Get(), PutRecordsZen);
			ValidateRecords(TEXT("SimpleValueWithMetaZenAndDirect"), GetAndValidateRecords(TEXT("SimpleValueWithMetaZen"), PutRecordsZen, ECachePolicy::Default), RecievedRecords, ECachePolicy::Default);
			ValidateRecords(TEXT("SimpleValueWithMetaSkipMetaZenAndDirect"), GetAndValidateRecords(TEXT("SimpleValueWithMetaSkipMetaZen"), PutRecordsZen, ECachePolicy::Default | ECachePolicy::SkipMeta), RecievedRecordsSkipMeta, ECachePolicy::Default | ECachePolicy::SkipMeta);
			ValidateRecords(TEXT("SimpleValueWithMetaSkipDataZenAndDirect"), GetAndValidateRecords(TEXT("SimpleValueWithMetaSkipDataZen"), PutRecordsZen, ECachePolicy::Default | ECachePolicy::SkipData), RecievedRecordsSkipData, ECachePolicy::Default | ECachePolicy::SkipData);
		}
#endif // UE_WITH_ZEN
	}

	{
		TArray<FCacheRecord> PutRecords = CreateTestCacheRecords(TestBackend, RecordsInBatch, 5);
		TArray<FCacheRecord> RecievedRecords = GetAndValidateRecordsAndChunks(TEXT("MultiValue"), PutRecords, ECachePolicy::Default);
		TArray<FCacheRecord> RecievedRecordsSkipMeta = GetAndValidateRecordsAndChunks(TEXT("MultiValueSkipMeta"), PutRecords, ECachePolicy::Default | ECachePolicy::SkipMeta);
		TArray<FCacheRecord> RecievedRecordsSkipData = GetAndValidateRecordsAndChunks(TEXT("MultiValueSkipData"), PutRecords, ECachePolicy::Default | ECachePolicy::SkipData);

#if UE_WITH_ZEN
		if (ZenIntermediaryBackend)
		{
			TArray<FCacheRecord> PutRecordsZen = CreateTestCacheRecords(ZenIntermediaryBackend.Get(), RecordsInBatch, 5, FCbObject(), "AutoTestDummyZen");
			WaitForZenPushToUpstream(ZenIntermediaryBackend.Get(), PutRecordsZen);
			ValidateRecords(TEXT("MultiValueZenAndDirect"), GetAndValidateRecords(TEXT("MultiValueZen"), PutRecordsZen, ECachePolicy::Default), RecievedRecords, ECachePolicy::Default);
			ValidateRecords(TEXT("MultiValueSkipMetaZenAndDirect"), GetAndValidateRecords(TEXT("MultiValueSkipMetaZen"), PutRecordsZen, ECachePolicy::Default | ECachePolicy::SkipMeta), RecievedRecordsSkipMeta, ECachePolicy::Default | ECachePolicy::SkipMeta);
			ValidateRecords(TEXT("MultiValueSkipDataZenAndDirect"), GetAndValidateRecords(TEXT("MultiValueSkipDataZen"), PutRecordsZen, ECachePolicy::Default | ECachePolicy::SkipData), RecievedRecordsSkipData, ECachePolicy::Default | ECachePolicy::SkipData);
		}
#endif // UE_WITH_ZEN
	}

	{
		TArray<FValue> PutValues = CreateTestCacheValues(TestBackend, ValuesInBatch);
		TArray<FValue> ReceivedValues = GetAndValidateValues(TEXT("SimpleValue"), PutValues, ECachePolicy::Default);
		TArray<FValue> ReceivedValuesSkipData = GetAndValidateValues(TEXT("SimpleValueSkipData"), PutValues, ECachePolicy::Default | ECachePolicy::SkipData);

#if UE_WITH_ZEN
		if (ZenIntermediaryBackend)
		{
			TArray<FValue> PutValuesZen = CreateTestCacheValues(ZenIntermediaryBackend.Get(), ValuesInBatch);
			WaitForZenPushValuesToUpstream(ZenIntermediaryBackend.Get(), PutValuesZen);
			ValidateValues(TEXT("SimpleValueZenAndDirect"), GetAndValidateValues(TEXT("SimpleValueZen"), PutValuesZen, ECachePolicy::Default), ReceivedValues, ECachePolicy::Default);
			ValidateValues(TEXT("SimpleValueSkipDataZenAndDirect"), GetAndValidateValues(TEXT("SimpleValueSkipDataZen"), PutValuesZen, ECachePolicy::Default | ECachePolicy::SkipData), ReceivedValuesSkipData, ECachePolicy::Default | ECachePolicy::SkipData);
		}
		if (ZenIntermediarySiblingBackend)
		{
			GetAndValidateValues(TEXT("SimpleValueZen"), PutValues, ECachePolicy::Default, ZenIntermediarySiblingBackend.Get());
			GetAndValidateValues(TEXT("SimpleValueSkipDataZen"), PutValues, ECachePolicy::Default | ECachePolicy::SkipData, ZenIntermediarySiblingBackend.Get());
		}
#endif // UE_WITH_ZEN
	}

	return true;
}

} // UE::DerivedData

#endif // #if WITH_DEV_AUTOMATION_TESTS && WITH_HTTP_DDC_BACKEND
