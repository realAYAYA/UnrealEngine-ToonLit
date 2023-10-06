// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "Hash/Blake3.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/StringBuilder.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCacheStoreTest, "System.DerivedDataCache.CacheStore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FCacheStoreTest::RunTest(const FString& Parameters)
{
	using namespace UE::DerivedData;
	ICache& Cache = GetCache();
	FStringView TestVersion = TEXTVIEW("D6B05C93623D46D891D354BB22FCB584");

	FRequestOwner Owner(EPriority::Blocking);
	FCacheBucket DDCTestBucket(TEXT("DDCTest"));
	static bool bExpectWarm = FParse::Param(FCommandLine::Get(), TEXT("CacheStoreTestWarm"));

	// NumKeys = (2 Value vs Record)*(2 SkipData vs Default)*(2 ForceMiss vs Not)*(2 use local)
	// *(2 use remote)*(2 UseValue Policy vs not)*(4 cases per type)
	constexpr int32 NumKeys = 256;
	constexpr int32 NumValues = 4;
	TArray<FCachePutRequest> PutRequests;
	TArray<FCachePutValueRequest> PutValueRequests;
	TArray<FCacheGetRequest> GetRequests;
	TArray<FCacheGetValueRequest> GetValueRequests;
	TArray<FCacheGetChunkRequest> ChunkRequests;
	FValueId ValueIds[NumValues];
	for (int32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
	{
		ValueIds[ValueIndex] = FValueId::FromName(*WriteToString<16>(TEXT("ValueId_"), ValueIndex));
	}

	struct FKeyData;
	struct FUserData
	{
		FUserData& Set(FKeyData* InKeyData, int32 InValueIndex)
		{
			KeyData = InKeyData;
			ValueIndex = InValueIndex;
			return *this;
		}
		FKeyData* KeyData = nullptr;
		int32 ValueIndex = 0;
	};
	struct FKeyData
	{
		FValue BufferValues[NumValues];
		uint64 IntValues[NumValues];
		FUserData ValueUserData[NumValues];
		bool ReceivedChunk[NumValues];
		FCacheKey Key;
		FUserData KeyUserData;
		uint32 KeyIndex = 0;
		bool bGetRequestsData = true;
		bool bUseValueAPI = false;
		bool bUseValuePolicy = false;
		bool bForceMiss = false;
		bool bUseLocal = true;
		bool bUseRemote = true;
		bool bReceivedPut = false;
		bool bReceivedGet = false;
		bool bReceivedPutValue = false;
		bool bReceivedGetValue = false;
	};

	FKeyData KeyDatas[NumKeys];
	for (uint32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		FBlake3 KeyWriter;
		KeyWriter.Update(*TestName, TestName.Len() * sizeof(TestName[0]));
		KeyWriter.Update(TestVersion.GetData(), TestVersion.Len() * sizeof(TestVersion[0]));
		KeyWriter.Update(&KeyIndex, sizeof(KeyIndex));
		FIoHash KeyHash = KeyWriter.Finalize();
		FSharedString Name(WriteToString<16>(TEXT("Key_"), KeyIndex));
		FKeyData& KeyData = KeyDatas[KeyIndex];

		KeyData.Key = FCacheKey{ DDCTestBucket, KeyHash };
		KeyData.KeyIndex = KeyIndex;
		KeyData.bGetRequestsData = (KeyIndex & (1 << 1)) == 0;
		KeyData.bUseValueAPI = (KeyIndex & (1 << 2)) != 0;
		KeyData.bUseValuePolicy = (KeyIndex & (1 << 3)) != 0;
		KeyData.bForceMiss = (KeyIndex & (1 << 4)) == 0;
		KeyData.bUseLocal = (KeyIndex & (1 << 5)) == 0;
		KeyData.bUseRemote = (KeyIndex & (1 << 6)) == 0;
		ECachePolicy SharedPolicy = KeyData.bUseLocal ? ECachePolicy::Local : ECachePolicy::None;
		SharedPolicy |= KeyData.bUseRemote ? ECachePolicy::Remote : ECachePolicy::None;
		ECachePolicy PutPolicy = SharedPolicy;
		ECachePolicy GetPolicy = SharedPolicy;
		GetPolicy |= !KeyData.bGetRequestsData ? ECachePolicy::SkipData : ECachePolicy::None;
		FCacheKey& Key = KeyData.Key;

		for (int32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
		{
			KeyData.IntValues[ValueIndex] = static_cast<uint64>(KeyIndex) | (static_cast<uint64>(ValueIndex) << 32);
			KeyData.BufferValues[ValueIndex] = FValue::Compress(FSharedBuffer::MakeView(&KeyData.IntValues[ValueIndex], sizeof(KeyData.IntValues[ValueIndex])));
			KeyData.ReceivedChunk[ValueIndex] = false;
		}

		static_assert(sizeof(uint64) == sizeof(FUserData*), "Storing pointers in the UserData");
		FUserData& KeyUserData = KeyData.KeyUserData.Set(&KeyData, -1);
		for (uint32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
		{
			KeyData.ValueUserData[ValueIndex].Set(&KeyData, ValueIndex);
		}
		if (!KeyData.bUseValueAPI)
		{
			FCacheRecordBuilder Builder(Key);
			for (uint32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
			{
				Builder.AddValue(ValueIds[ValueIndex], KeyData.BufferValues[ValueIndex]);
			}

			FCacheRecordPolicy PutRecordPolicy;
			FCacheRecordPolicy GetRecordPolicy;
			if (!KeyData.bUseValuePolicy)
			{
				PutRecordPolicy = FCacheRecordPolicy(PutPolicy);
				GetRecordPolicy = FCacheRecordPolicy(GetPolicy);
			}
			else
			{
				// Switch the SkipData field in the Record policy so that if the CacheStore ignores the ValuePolicies
				// it will use the wrong value for SkipData and fail our tests.
				FCacheRecordPolicyBuilder PutBuilder(PutPolicy ^ ECachePolicy::SkipData);
				FCacheRecordPolicyBuilder GetBuilder(GetPolicy ^ ECachePolicy::SkipData);
				for (int32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
				{
					PutBuilder.AddValuePolicy(ValueIds[ValueIndex], PutPolicy);
					GetBuilder.AddValuePolicy(ValueIds[ValueIndex], GetPolicy);
				}
				PutRecordPolicy = PutBuilder.Build();
				GetRecordPolicy = GetBuilder.Build();
			}
			if (!KeyData.bForceMiss)
			{
				PutRequests.Add(FCachePutRequest{ Name, Builder.Build(), PutRecordPolicy, reinterpret_cast<uint64>(&KeyUserData) });
			}
			GetRequests.Add(FCacheGetRequest{ Name, Key, GetRecordPolicy, reinterpret_cast<uint64>(&KeyUserData) });
			for (uint32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
			{
				FUserData& ValueUserData = KeyData.ValueUserData[ValueIndex];
				ChunkRequests.Add(FCacheGetChunkRequest{ Name, Key, ValueIds[ValueIndex],
					0, MAX_uint64, FIoHash(), GetPolicy, reinterpret_cast<uint64>(&ValueUserData) });
			}
		}
		else
		{
			if (!KeyData.bForceMiss)
			{
				PutValueRequests.Add(FCachePutValueRequest{ Name, Key, KeyData.BufferValues[0], PutPolicy, reinterpret_cast<uint64>(&KeyUserData) });
			}
			GetValueRequests.Add(FCacheGetValueRequest{ Name, Key, GetPolicy, reinterpret_cast<uint64>(&KeyUserData) });
			ChunkRequests.Add(FCacheGetChunkRequest{ Name, Key, FValueId(),
				0, MAX_uint64, FIoHash(), GetPolicy, reinterpret_cast<uint64>(&KeyUserData) });
		}
	}

	bool bLocalPutSucceeded = true;
	bool bRemotePutSucceeded = true;
	if (!bExpectWarm)
	{
		bool bLocalPutSucceededInitialized = false;
		bool bRemotePutSucceededInitialized = false;
		{
			FRequestBarrier Barrier(Owner);
			Cache.Put(PutRequests, Owner,
				[this, &bLocalPutSucceededInitialized, &bLocalPutSucceeded, &bRemotePutSucceededInitialized,
				&bRemotePutSucceeded]
			(FCachePutResponse&& Response)
				{
					FUserData* UserData = reinterpret_cast<FUserData*>(Response.UserData);
					FKeyData* KeyData = UserData->KeyData;
					TestTrue(TEXT("Valid UserData in Put Callback"), KeyData != nullptr);
					if (KeyData)
					{
						if (KeyData->bUseLocal && !KeyData->bUseRemote && !bLocalPutSucceededInitialized)
						{
							bLocalPutSucceededInitialized = true;
							bLocalPutSucceeded = Response.Status == UE::DerivedData::EStatus::Ok;
						}
						if (!KeyData->bUseLocal && KeyData->bUseRemote && !bRemotePutSucceededInitialized)
						{
							bRemotePutSucceededInitialized = true;
							bRemotePutSucceeded = Response.Status == UE::DerivedData::EStatus::Ok;
						}
						KeyData->bReceivedPut = true;
					}
				});
			Cache.PutValue(PutValueRequests, Owner, [this](FCachePutValueResponse&& Response)
				{
					FUserData* UserData = reinterpret_cast<FUserData*>(Response.UserData);
					FKeyData* KeyData = UserData->KeyData;
					TestTrue(TEXT("Valid UserData in PutValue Callback"), KeyData != nullptr);
					if (KeyData)
					{
						KeyData->bReceivedPutValue = true;
					}
				});
		}
		Owner.Wait();
		for (FKeyData& KeyData : KeyDatas)
		{
			int32 n = KeyData.KeyIndex;
			if (!KeyData.bForceMiss)
			{
				if (!KeyData.bUseValueAPI)
				{
					TestTrue(*WriteToString<16>(TEXT("Put "), n, TEXT(" received")), KeyData.bReceivedPut);
				}
				else
				{
					TestTrue(*WriteToString<16>(TEXT("PutValue "), n, TEXT(" received")), KeyData.bReceivedPutValue);
				}
			}
		}
	}

	{
		FRequestBarrier Barrier(Owner);
		Cache.Get(GetRequests, Owner, [&ValueIds, this, NumValues, bLocalPutSucceeded, bRemotePutSucceeded](FCacheGetResponse&& Response)
			{
				FUserData* UserData = reinterpret_cast<FUserData*>(Response.UserData);
				FKeyData* KeyData = UserData->KeyData;
				TestTrue(TEXT("Valid UserData in Get Callback"), KeyData != nullptr);
				if (KeyData)
				{
					int32 n = (int32)KeyData->KeyIndex;
					KeyData->bReceivedGet = true;

					bool bShouldBeHit = !KeyData->bForceMiss && ((KeyData->bUseLocal && bLocalPutSucceeded) ||
						(KeyData->bUseRemote && bRemotePutSucceeded));
					if (bShouldBeHit)
					{
						TestEqual(*WriteToString<32>(TEXT("Get "), n, TEXT(" succeeded")), Response.Status, EStatus::Ok);
					}
					else if (KeyData->bForceMiss)
					{
						TestEqual(*WriteToString<32>(TEXT("Get "), n, TEXT(" failed as expected")), Response.Status, EStatus::Error);
					}
					if (!KeyData->bForceMiss && Response.Status == EStatus::Ok)
					{
						FCacheRecord& Record = Response.Record;
						TConstArrayView<FValueWithId> Values = Record.GetValues();
						if (TestEqual(*WriteToString<32>(TEXT("Get "), n, TEXT(" ValuesLen")), Values.Num(), NumValues))
						{
							for (int32 ActualValueIndex = 0; ActualValueIndex < NumValues; ++ActualValueIndex)
							{
								const FValueWithId& ActualValue = Values[ActualValueIndex];
								int32 ExpectedValueIndex = TArrayView<const FValueId>(ValueIds).Find(ActualValue.GetId());
								const FValueId& ExpectedValueId = ExpectedValueIndex >= 0 ? ValueIds[ExpectedValueIndex] : FValueId();

								TestTrue(*WriteToString<32>(TEXT("Get "), n, TEXT(" ValueId")), ExpectedValueIndex >= 0);
								if (ExpectedValueIndex >= 0)
								{
									FValue& ExpectedValue = KeyData->BufferValues[ExpectedValueIndex];
									TestEqual(*WriteToString<32>(TEXT("Get "), n, TEXT(" Hash")),
										ActualValue.GetRawHash(), ExpectedValue.GetRawHash());
									TestEqual(*WriteToString<32>(TEXT("Get "), n, TEXT(" Size")),
										ActualValue.GetRawSize(), ExpectedValue.GetRawSize());

									if (KeyData->bGetRequestsData)
									{
										const FCompressedBuffer& Compressed = ActualValue.GetData();
										FSharedBuffer Buffer = Compressed.Decompress();
										TestEqual(*WriteToString<32>(TEXT("Get "), n, TEXT(" Data Size")),
											Buffer.GetSize(), ActualValue.GetRawSize());
										if (Buffer.GetSize())
										{
											uint64 ActualIntValue = ((const uint64*)Buffer.GetData())[0];
											uint64 ExpectedIntValue = KeyData->IntValues[ExpectedValueIndex];
											TestEqual(*WriteToString<32>(TEXT("Get "), n, TEXT(" Data Equals"), n),
												ActualIntValue, ExpectedIntValue);
										}
									}
								}
							}
						}
					}
				}
			});

		Cache.GetValue(GetValueRequests, Owner, [this, bLocalPutSucceeded, bRemotePutSucceeded](FCacheGetValueResponse&& Response)
			{
				FUserData* UserData = reinterpret_cast<FUserData*>(Response.UserData);
				FKeyData* KeyData = UserData->KeyData;
				TestTrue(TEXT("Valid UserData in GetValue Callback"), KeyData != nullptr);
				if (KeyData)
				{
					int32 n = KeyData->KeyIndex;
					KeyData->bReceivedGetValue = true;

					bool bShouldBeHit = !KeyData->bForceMiss && ((KeyData->bUseLocal && bLocalPutSucceeded) ||
						(KeyData->bUseRemote && bRemotePutSucceeded));
					if (bShouldBeHit)
					{
						TestEqual(*WriteToString<32>(TEXT("GetValue "), n, TEXT(" succeeded")), Response.Status, EStatus::Ok);
					}
					else if (KeyData->bForceMiss)
					{
						TestEqual(*WriteToString<32>(TEXT("GetValue "), n, TEXT(" failed as expected")), Response.Status, EStatus::Error);
					}
					if (!KeyData->bForceMiss && Response.Status == EStatus::Ok)
					{
						FValue& ActualValue = Response.Value;
						FValue& ExpectedValue = KeyData->BufferValues[0];
						TestEqual(*WriteToString<32>(TEXT("GetValue "), n, TEXT(" Hash")),
							ActualValue.GetRawHash(), ExpectedValue.GetRawHash());
						TestEqual(*WriteToString<32>(TEXT("GetValue "), n, TEXT(" Size")),
							ActualValue.GetRawSize(), ExpectedValue.GetRawSize());

						if (KeyData->bGetRequestsData)
						{
							const FCompressedBuffer& Compressed = ActualValue.GetData();
							FSharedBuffer Buffer = Compressed.Decompress();
							TestEqual(*WriteToString<32>(TEXT("GetValue "), n, TEXT(" Data Size")),
								Buffer.GetSize(), ActualValue.GetRawSize());
							if (Buffer.GetSize())
							{
								uint64 Value = ((const uint64*)Buffer.GetData())[0];
								TestEqual(*WriteToString<32>(TEXT("GetValue "), n, TEXT(" Data Equals")),
									Value, KeyData->IntValues[0]);
							}
						}
					}
				}
			});

		Cache.GetChunks(ChunkRequests, Owner, [this, bLocalPutSucceeded, bRemotePutSucceeded](FCacheGetChunkResponse&& Response)
			{
				FUserData* UserData = reinterpret_cast<FUserData*>(Response.UserData);
				FKeyData* KeyData = UserData->KeyData;
				TestTrue(TEXT("Valid UserData in GetChunks Callback"), KeyData != nullptr);
				if (KeyData != nullptr)
				{
					int32 n = KeyData->KeyIndex;
					int32 ValueIndex = UserData->ValueIndex >= 0 ? UserData->ValueIndex : 0;
					TStringBuilder<32> Name;
					Name << TEXT("GetChunks (") << n << TEXT(",") << ValueIndex << TEXT(")");

					KeyData->ReceivedChunk[ValueIndex] = true;
					bool bShouldBeHit = !KeyData->bForceMiss && ((KeyData->bUseLocal && bLocalPutSucceeded) ||
						(KeyData->bUseRemote && bRemotePutSucceeded));
					if (bShouldBeHit)
					{
						TestEqual(*WriteToString<32>(*Name, TEXT(" succeeded")), Response.Status, EStatus::Ok);
					}
					else if (KeyData->bForceMiss)
					{
						TestEqual(*WriteToString<32>(*Name, TEXT(" failed as expected")), Response.Status, EStatus::Error);
					}
					if (bShouldBeHit && Response.Status == EStatus::Ok)
					{
						FValue& ExpectedValue = KeyData->BufferValues[ValueIndex];
						TestEqual(*WriteToString<32>(*Name, TEXT(" Hash")),
							Response.RawHash, ExpectedValue.GetRawHash());
						TestEqual(*WriteToString<32>(*Name, TEXT(" Size")),
							Response.RawSize, ExpectedValue.GetRawSize());
						if (KeyData->bGetRequestsData)
						{
							FSharedBuffer Buffer = Response.RawData;
							TestEqual(*WriteToString<32>(*Name, TEXT(" Data Size")),
								Buffer.GetSize(), Response.RawSize);
							if (Buffer.GetSize())
							{
								uint64 Value = ((const uint64*)Buffer.GetData())[0];
								TestEqual(*WriteToString<32>(*Name, TEXT(" Data Equals")),
									Value, KeyData->IntValues[ValueIndex]);
							}
						}
					}
				}
			});
	}
	Owner.Wait();
	for (FKeyData& KeyData : KeyDatas)
	{
		int32 n = KeyData.KeyIndex;
		if (!KeyData.bUseValueAPI)
		{
			TestTrue(*WriteToString<32>(TEXT("Get "), n, TEXT(" received")), KeyData.bReceivedGet);
			for (int32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
			{
				TestTrue(*WriteToString<32>(TEXT("GetChunk ("), n, TEXT(","), ValueIndex, TEXT(") received")), KeyData.ReceivedChunk[ValueIndex]);
			}
		}
		else
		{
			TestTrue(*WriteToString<32>(TEXT("GetValue "), n, TEXT(" received")), KeyData.bReceivedGetValue);
			TestTrue(*WriteToString<32>(TEXT("GetChunk ("), n, TEXT(",0) received")), KeyData.ReceivedChunk[0]);
		}
	}

	return true;
}

#endif