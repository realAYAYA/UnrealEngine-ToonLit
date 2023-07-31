// Copyright Epic Games, Inc. All Rights Reserved.

#include "Algo/Compare.h"
#include "Containers/Set.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataCacheKeyFilter.h"
#include "DerivedDataLegacyCacheStore.h"
#include "HAL/CriticalSection.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/FileHelper.h"
#include "String/Find.h"
#include "Templates/Tuple.h"
#include "Templates/UniquePtr.h"

namespace UE::DerivedData
{

/**
 * A cache store that verifies that derived data is generated deterministically.
 *
 * This wraps a cache store and fails every get until a matching put occurs, then compares the derived data.
 */
class FCacheStoreVerify : public ILegacyCacheStore
{
public:
	FCacheStoreVerify(ILegacyCacheStore* InInnerCache, bool bInPutOnError)
		: InnerCache(InInnerCache)
		, bPutOnError(bInPutOnError)
	{
		check(InnerCache);

		const TCHAR* const CommandLine = FCommandLine::Get();

		const bool bDefaultMatch = FParse::Param(CommandLine, TEXT("DDC-Verify")) ||
			String::FindFirst(CommandLine, TEXT("-DDC-Verify="), ESearchCase::IgnoreCase) == INDEX_NONE;
		float DefaultRate = bDefaultMatch ? 100.0f : 0.0f;
		FParse::Value(CommandLine, TEXT("-DDC-VerifyRate="), DefaultRate);

		Filter = FCacheKeyFilter::Parse(CommandLine, TEXT("-DDC-Verify="), DefaultRate);

		uint32 Salt;
		if (FParse::Value(CommandLine, TEXT("-DDC-VerifySalt="), Salt))
		{
			if (Salt == 0)
			{
				UE_LOG(LogDerivedDataCache, Warning,
					TEXT("Verify: Ignoring salt of 0. The salt must be a positive integer."));
			}
			else
			{
				Filter.SetSalt(Salt);
			}
		}

		if (Filter)
		{
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("Verify: Using salt -DDC-VerifySalt=%u to filter cache keys to verify."), Filter.GetSalt());
		}
	}

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) final;

	void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final;

	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) final;

	void GetValue(
		TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete) final;

	void GetChunks(
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) final;

	void LegacyStats(FDerivedDataCacheStatsNode& OutNode) final
	{
		InnerCache->LegacyStats(OutNode);
	}

	bool LegacyDebugOptions(FBackendDebugOptions& Options) final
	{
		return InnerCache->LegacyDebugOptions(Options);
	}

private:
	struct FVerifyPutState
	{
		TArray<FCachePutRequest> ForwardRequests;
		TArray<FCachePutRequest> VerifyRequests;
		FOnCachePutComplete OnComplete;
		int32 ActiveRequests = 0;
		FRWLock Lock;
	};

	struct FVerifyPutValueState
	{
		TArray<FCachePutValueRequest> ForwardRequests;
		TArray<FCachePutValueRequest> VerifyRequests;
		FOnCachePutValueComplete OnComplete;
		int32 ActiveRequests = 0;
		FRWLock Lock;
	};

	void GetMetaComplete(IRequestOwner& Owner, FVerifyPutState* State, FCacheGetResponse&& Response);
	void GetDataComplete(IRequestOwner& Owner, FVerifyPutState* State, FCacheGetResponse&& Response);
	void GetComplete(IRequestOwner& Owner, FVerifyPutState* State);

	void GetMetaComplete(IRequestOwner& Owner, FVerifyPutValueState* State, FCacheGetValueResponse&& Response);
	void GetDataComplete(IRequestOwner& Owner, FVerifyPutValueState* State, FCacheGetValueResponse&& Response);
	void GetComplete(IRequestOwner& Owner, FVerifyPutValueState* State);

	static bool CompareRecords(const FCacheRecord& PutRecord, const FCacheRecord& GetRecord, const FSharedString& Name);

	static void LogChangedValue(
		const FSharedString& Name,
		const FCacheKey& Key,
		const FValueId& Id,
		const FIoHash& NewRawHash,
		const FIoHash& OldRawHash,
		const FCompositeBuffer& NewRawData,
		const FCompositeBuffer& OldRawData);

private:
	ILegacyCacheStore* InnerCache;
	FCriticalSection AlreadyTestedLock;
	TSet<FCacheKey> AlreadyTested;
	FCacheKeyFilter Filter;
	bool bPutOnError;
};

void FCacheStoreVerify::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	TUniquePtr<FVerifyPutState> State = MakeUnique<FVerifyPutState>();
	State->VerifyRequests.Reserve(Requests.Num());
	{
		FScopeLock Lock(&AlreadyTestedLock);
		for (const FCachePutRequest& Request : Requests)
		{
			const FCacheKey& Key = Request.Record.GetKey();
			bool bForward = !Filter.IsMatch(Key);
			if (!bForward)
			{
				AlreadyTested.Add(Key, &bForward);
			}
			(bForward ? State->ForwardRequests : State->VerifyRequests).Add(Request);
		}
	}

	if (State->VerifyRequests.IsEmpty())
	{
		return InnerCache->Put(State->ForwardRequests, Owner, MoveTemp(OnComplete));
	}

	TArray<FCacheGetRequest> GetMetaRequests;
	GetMetaRequests.Reserve(State->VerifyRequests.Num());
	{
		uint64 PutIndex = 0;
		const ECachePolicy GetPolicy = ECachePolicy::Query | ECachePolicy::PartialRecord | ECachePolicy::SkipData;
		for (const FCachePutRequest& PutRequest : State->VerifyRequests)
		{
			GetMetaRequests.Add({PutRequest.Name, PutRequest.Record.GetKey(), GetPolicy, PutIndex++});
		}
	}

	State->OnComplete = MoveTemp(OnComplete);
	State->ActiveRequests = GetMetaRequests.Num();
	InnerCache->Get(GetMetaRequests, Owner, [this, &Owner, State = State.Release()](FCacheGetResponse&& MetaResponse)
	{
		GetMetaComplete(Owner, State, MoveTemp(MetaResponse));
	});
}

void FCacheStoreVerify::GetMetaComplete(IRequestOwner& Owner, FVerifyPutState* State, FCacheGetResponse&& Response)
{
	FCachePutRequest& Request = State->VerifyRequests[int32(Response.UserData)];

	if ((Response.Status == EStatus::Ok) ||
		(Response.Status == EStatus::Error && !Response.Record.GetValues().IsEmpty()))
	{
		const auto MakeValueTuple = [](const FValueWithId& Value) -> TTuple<FValueId, FIoHash>
		{
			return MakeTuple(Value.GetId(), Value.GetRawHash());
		};
		if (Algo::CompareBy(Request.Record.GetValues(), Response.Record.GetValues(), MakeValueTuple))
		{
			UE_LOG(LogDerivedDataCache, Verbose,
				TEXT("Verify: Data in the cache matches newly generated data for %s from '%s'."),
				*WriteToString<96>(Request.Record.GetKey()), *Request.Name);
			State->OnComplete(Request.MakeResponse(EStatus::Ok));
		}
		else
		{
			const ECachePolicy Policy = ECachePolicy::Default | ECachePolicy::PartialRecord;
			const FCacheGetRequest GetDataRequests[]{{Response.Name, Response.Record.GetKey(), Policy, Response.UserData}};
			return InnerCache->Get(GetDataRequests, Owner, [this, &Owner, State](FCacheGetResponse&& DataResponse)
			{
				GetDataComplete(Owner, State, MoveTemp(DataResponse));
			});
		}
	}
	else
	{
		UE_LOG(LogDerivedDataCache, Warning,
			TEXT("Verify: Cache did not contain a record for %s from '%s'."),
			*WriteToString<96>(Request.Record.GetKey()), *Request.Name);
		FWriteScopeLock Lock(State->Lock);
		State->ForwardRequests.Add(MoveTemp(Request));
	}

	GetComplete(Owner, State);
}

void FCacheStoreVerify::GetDataComplete(IRequestOwner& Owner, FVerifyPutState* State, FCacheGetResponse&& Response)
{
	FCachePutRequest& Request = State->VerifyRequests[int32(Response.UserData)];

	if ((Response.Status == EStatus::Ok) ||
		(Response.Status == EStatus::Error && !Response.Record.GetValues().IsEmpty()))
	{
		if (CompareRecords(Request.Record, Response.Record, Request.Name))
		{
			UE_LOG(LogDerivedDataCache, Verbose,
				TEXT("Verify: Data in the cache matches newly generated data for %s from '%s'."),
				*WriteToString<96>(Request.Record.GetKey()), *Request.Name);
			State->OnComplete(Request.MakeResponse(EStatus::Ok));
		}
		else if (bPutOnError)
		{
			// Ask to overwrite existing records to potentially eliminate the mismatch.
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("Verify: Writing newly generated data to the cache for %s from '%s'."),
				*WriteToString<96>(Request.Record.GetKey()), *Request.Name);
			Request.Policy = Request.Policy.Transform([](ECachePolicy P) { return P & ~ECachePolicy::Query; });
			FWriteScopeLock Lock(State->Lock);
			State->ForwardRequests.Add(MoveTemp(Request));
		}
		else
		{
			State->OnComplete(Request.MakeResponse(EStatus::Ok));
		}
	}
	else
	{
		UE_LOG(LogDerivedDataCache, Warning,
			TEXT("Verify: Cache did not contain a record for %s from '%s'."),
			*WriteToString<96>(Request.Record.GetKey()), *Request.Name);
		FWriteScopeLock Lock(State->Lock);
		State->ForwardRequests.Add(MoveTemp(Request));
	}

	GetComplete(Owner, State);
}

void FCacheStoreVerify::GetComplete(IRequestOwner& Owner, FVerifyPutState* State)
{
	if (FWriteScopeLock Lock(State->Lock); --State->ActiveRequests > 0)
	{
		return;
	}
	if (!State->ForwardRequests.IsEmpty())
	{
		InnerCache->Put(State->ForwardRequests, Owner, MoveTemp(State->OnComplete));
	}
	delete State;
}

void FCacheStoreVerify::Get(
	const TConstArrayView<FCacheGetRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetComplete&& OnComplete)
{
	TArray<FCacheGetRequest, TInlineAllocator<8>> ForwardRequests;
	TArray<FCacheGetRequest, TInlineAllocator<8>> VerifyRequests;
	ForwardRequests.Reserve(Requests.Num());
	VerifyRequests.Reserve(Requests.Num());
	{
		FScopeLock Lock(&AlreadyTestedLock);
		for (const FCacheGetRequest& Request : Requests)
		{
			const bool bForward = !Filter.IsMatch(Request.Key) || AlreadyTested.Contains(Request.Key);
			(bForward ? ForwardRequests : VerifyRequests).Add(Request);
		}
	}

	CompleteWithStatus(VerifyRequests, OnComplete, EStatus::Error);

	if (!ForwardRequests.IsEmpty())
	{
		InnerCache->Get(ForwardRequests, Owner, MoveTemp(OnComplete));
	}
}

void FCacheStoreVerify::PutValue(
	const TConstArrayView<FCachePutValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutValueComplete&& OnComplete)
{
	TUniquePtr<FVerifyPutValueState> State = MakeUnique<FVerifyPutValueState>();
	State->VerifyRequests.Reserve(Requests.Num());
	{
		FScopeLock Lock(&AlreadyTestedLock);
		for (const FCachePutValueRequest& Request : Requests)
		{
			bool bForward = !Filter.IsMatch(Request.Key);
			if (!bForward)
			{
				AlreadyTested.Add(Request.Key, &bForward);
			}
			(bForward ? State->ForwardRequests : State->VerifyRequests).Add(Request);
		}
	}

	if (State->VerifyRequests.IsEmpty())
	{
		return InnerCache->PutValue(State->ForwardRequests, Owner, MoveTemp(OnComplete));
	}

	TArray<FCacheGetValueRequest> GetMetaRequests;
	GetMetaRequests.Reserve(State->VerifyRequests.Num());
	{
		uint64 PutIndex = 0;
		const ECachePolicy GetPolicy = ECachePolicy::Query | ECachePolicy::SkipData;
		for (const FCachePutValueRequest& PutRequest : State->VerifyRequests)
		{
			GetMetaRequests.Add({PutRequest.Name, PutRequest.Key, GetPolicy, PutIndex++});
		}
	}

	State->OnComplete = MoveTemp(OnComplete);
	State->ActiveRequests = GetMetaRequests.Num();
	InnerCache->GetValue(GetMetaRequests, Owner, [this, &Owner, State = State.Release()](FCacheGetValueResponse&& MetaResponse)
	{
		GetMetaComplete(Owner, State, MoveTemp(MetaResponse));
	});
}

void FCacheStoreVerify::GetMetaComplete(IRequestOwner& Owner, FVerifyPutValueState* State, FCacheGetValueResponse&& Response)
{
	FCachePutValueRequest& Request = State->VerifyRequests[int32(Response.UserData)];

	if (Response.Status == EStatus::Ok)
	{
		if (Request.Value.GetRawHash() == Response.Value.GetRawHash())
		{
			UE_LOG(LogDerivedDataCache, Verbose,
				TEXT("Verify: Data in the cache matches newly generated data for %s from '%s'."),
				*WriteToString<96>(Request.Key), *Request.Name);
			State->OnComplete(Request.MakeResponse(EStatus::Ok));
		}
		else
		{
			const FCacheGetValueRequest GetDataRequests[]{{Response.Name, Response.Key, ECachePolicy::Default, Response.UserData}};
			return InnerCache->GetValue(GetDataRequests, Owner, [this, &Owner, State](FCacheGetValueResponse&& DataResponse)
			{
				GetDataComplete(Owner, State, MoveTemp(DataResponse));
			});
		}
	}
	else
	{
		UE_LOG(LogDerivedDataCache, Display,
			TEXT("Verify: Cache did not contain a value for %s from '%s'."),
			*WriteToString<96>(Request.Key), *Request.Name);
		FWriteScopeLock Lock(State->Lock);
		State->ForwardRequests.Add(MoveTemp(Request));
	}

	GetComplete(Owner, State);
}

void FCacheStoreVerify::GetDataComplete(IRequestOwner& Owner, FVerifyPutValueState* State, FCacheGetValueResponse&& Response)
{
	FCachePutValueRequest& Request = State->VerifyRequests[int32(Response.UserData)];

	if (Response.Status == EStatus::Ok)
	{
		if (Request.Value.GetRawHash() == Response.Value.GetRawHash())
		{
			UE_LOG(LogDerivedDataCache, Verbose,
				TEXT("Verify: Data in the cache matches newly generated data for %s from '%s'."),
				*WriteToString<96>(Request.Key), *Request.Name);
			State->OnComplete(Request.MakeResponse(EStatus::Ok));
		}
		else
		{
			LogChangedValue(Request.Name, Request.Key, FValueId::Null,
				Request.Value.GetRawHash(), Response.Value.GetRawHash(),
				Request.Value.GetData().DecompressToComposite(), Response.Value.GetData().DecompressToComposite());
			if (bPutOnError)
			{
				// Ask to overwrite existing values to potentially eliminate the mismatch.
				UE_LOG(LogDerivedDataCache, Display,
					TEXT("Verify: Writing newly generated data to the cache for %s from '%s'."),
					*WriteToString<96>(Request.Key), *Request.Name);
				Request.Policy &= ~ECachePolicy::Query;
				FWriteScopeLock Lock(State->Lock);
				State->ForwardRequests.Add(MoveTemp(Request));
			}
			else
			{
				State->OnComplete(Request.MakeResponse(EStatus::Ok));
			}
		}
	}
	else
	{
		UE_LOG(LogDerivedDataCache, Display,
			TEXT("Verify: Cache did not contain a value for %s from '%s'."),
			*WriteToString<96>(Request.Key), *Request.Name);
		FWriteScopeLock Lock(State->Lock);
		State->ForwardRequests.Add(MoveTemp(Request));
	}

	GetComplete(Owner, State);
}

void FCacheStoreVerify::GetComplete(IRequestOwner& Owner, FVerifyPutValueState* State)
{
	if (FWriteScopeLock Lock(State->Lock); --State->ActiveRequests > 0)
	{
		return;
	}
	if (!State->ForwardRequests.IsEmpty())
	{
		InnerCache->PutValue(State->ForwardRequests, Owner, MoveTemp(State->OnComplete));
	}
	delete State;
}

void FCacheStoreVerify::GetValue(
	const TConstArrayView<FCacheGetValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetValueComplete&& OnComplete)
{
	TArray<FCacheGetValueRequest, TInlineAllocator<8>> ForwardRequests;
	TArray<FCacheGetValueRequest, TInlineAllocator<8>> VerifyRequests;
	ForwardRequests.Reserve(Requests.Num());
	VerifyRequests.Reserve(Requests.Num());
	{
		FScopeLock Lock(&AlreadyTestedLock);
		for (const FCacheGetValueRequest& Request : Requests)
		{
			const bool bForward = !Filter.IsMatch(Request.Key) || AlreadyTested.Contains(Request.Key);
			(bForward ? ForwardRequests : VerifyRequests).Add(Request);
		}
	}

	CompleteWithStatus(VerifyRequests, OnComplete, EStatus::Error);

	if (!ForwardRequests.IsEmpty())
	{
		InnerCache->GetValue(ForwardRequests, Owner, MoveTemp(OnComplete));
	}
}

void FCacheStoreVerify::GetChunks(
	const TConstArrayView<FCacheGetChunkRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetChunkComplete&& OnComplete)
{
	TArray<FCacheGetChunkRequest, TInlineAllocator<8>> ForwardRequests;
	TArray<FCacheGetChunkRequest, TInlineAllocator<8>> VerifyRequests;
	ForwardRequests.Reserve(Requests.Num());
	VerifyRequests.Reserve(Requests.Num());
	{
		FScopeLock Lock(&AlreadyTestedLock);
		for (const FCacheGetChunkRequest& Request : Requests)
		{
			const bool bForward = !Filter.IsMatch(Request.Key) || AlreadyTested.Contains(Request.Key);
			(bForward ? ForwardRequests : VerifyRequests).Add(Request);
		}
	}

	CompleteWithStatus(VerifyRequests, OnComplete, EStatus::Error);

	if (!ForwardRequests.IsEmpty())
	{
		InnerCache->GetChunks(ForwardRequests, Owner, MoveTemp(OnComplete));
	}
}

bool FCacheStoreVerify::CompareRecords(const FCacheRecord& PutRecord, const FCacheRecord& GetRecord, const FSharedString& Name)
{
	bool bEqual = true;

	const FCacheKey& Key = PutRecord.GetKey();
	const TConstArrayView<FValueWithId> PutValues = PutRecord.GetValues();
	const TConstArrayView<FValueWithId> GetValues = GetRecord.GetValues();
	const FValueWithId* PutIt = PutValues.GetData();
	const FValueWithId* GetIt = GetValues.GetData();
	const FValueWithId* const PutEnd = PutIt + PutValues.Num();
	const FValueWithId* const GetEnd = GetIt + GetValues.Num();

	const auto LogNewValue = [&Name, &Key](const FValueWithId& Value)
	{
		UE_LOG(LogDerivedDataCache, Error,
			TEXT("Verify: Value %s with hash %s is in the new record but does not exist in the cache for %s from '%s'."),
			*WriteToString<32>(Value.GetId()), *WriteToString<48>(Value.GetRawHash()), *WriteToString<96>(Key), *Name);
	};

	const auto LogOldValue = [&Name, &Key](const FValueWithId& Value)
	{
		UE_LOG(LogDerivedDataCache, Error,
			TEXT("Verify: Value %s with hash %s is in the cache but does not exist in the new record for %s from '%s'."),
			*WriteToString<32>(Value.GetId()), *WriteToString<48>(Value.GetRawHash()), *WriteToString<96>(Key), *Name);
	};

	while (PutIt != PutEnd && GetIt != GetEnd)
	{
		if (PutIt->GetId() == GetIt->GetId())
		{
			if (PutIt->GetRawHash() != GetIt->GetRawHash())
			{
				LogChangedValue(Name, Key, PutIt->GetId(),
					PutIt->GetRawHash(), GetIt->GetRawHash(),
					PutIt->GetData().DecompressToComposite(), GetIt->GetData().DecompressToComposite());
				bEqual = false;
			}
			++PutIt;
			++GetIt;
		}
		else if (PutIt->GetId() < GetIt->GetId())
		{
			LogNewValue(*PutIt++);
			bEqual = false;
		}
		else
		{
			LogOldValue(*GetIt++);
			bEqual = false;
		}
	}

	while (PutIt != PutEnd)
	{
		LogNewValue(*PutIt++);
		bEqual = false;
	}

	while (GetIt != GetEnd)
	{
		LogOldValue(*GetIt++);
		bEqual = false;
	}

	return bEqual;
}

void FCacheStoreVerify::LogChangedValue(
	const FSharedString& Name,
	const FCacheKey& Key,
	const FValueId& Id,
	const FIoHash& NewRawHash,
	const FIoHash& OldRawHash,
	const FCompositeBuffer& NewRawData,
	const FCompositeBuffer& OldRawData)
{
	TStringBuilder<32> IdString;
	if (Id.IsValid())
	{
		IdString << TEXT(' ') << Id;
	}

	const auto LogDataToFile = [&Name, &Key, &Id, &IdString](const FIoHash& RawHash, const FCompositeBuffer& RawData, FStringView Extension)
	{
		if (!RawData.IsNull())
		{
			TStringBuilder<256> Path;
			FPathViews::Append(Path, FPaths::ProjectSavedDir(), TEXT("VerifyDDC"), TEXT(""));
			Path << Key.Bucket << TEXT('_') << Key.Hash;
			if (Id.IsValid())
			{
				Path << TEXT('_') << Id;
			}
			Path << Extension;
			if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_Silent)})
			{
				for (const FSharedBuffer& Segment : RawData.GetSegments())
				{
					Ar->Serialize(const_cast<void*>(Segment.GetData()), int64(Segment.GetSize()));
				}
			}
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Log,
				TEXT("Verify: Value%s does not have data with hash %s to save to disk for %s from '%s'."),
				*IdString, *WriteToString<48>(RawHash), *WriteToString<96>(Key), *Name);
		}
	};

	UE_LOG(LogDerivedDataCache, Error,
		TEXT("Verify: Value%s has hash %s in the newly generated data and hash %s in the cache for %s from '%s'."),
		*IdString, *WriteToString<48>(NewRawHash), *WriteToString<48>(OldRawHash), *WriteToString<96>(Key), *Name);
	LogDataToFile(NewRawHash, NewRawData, TEXTVIEW(".verify"));
	LogDataToFile(OldRawHash, OldRawData, TEXTVIEW(".fromcache"));
}

ILegacyCacheStore* CreateCacheStoreVerify(ILegacyCacheStore* InnerCache, bool bPutOnError)
{
	return new FCacheStoreVerify(InnerCache, bPutOnError);
}

} // UE::DerivedData
