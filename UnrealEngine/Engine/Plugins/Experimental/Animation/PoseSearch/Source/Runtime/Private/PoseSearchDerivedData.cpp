// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"

#include "UObject/Class.h"

#if WITH_EDITOR
#include "Serialization/BulkDataRegistry.h"
#include "PoseSearch/PoseSearchDerivedDataKey.h"
#endif // WITH_EDITOR

#if UE_BUILD_DEBUG && WITH_EDITOR
#define UE_POSE_SEARCH_DERIVED_DATA_LOGGING 1
#endif

#ifndef UE_POSE_SEARCH_DERIVED_DATA_LOGGING
#define UE_POSE_SEARCH_DERIVED_DATA_LOGGING 0
#endif


#if WITH_EDITOR
namespace UE::PoseSearch
{
	const UE::DerivedData::FValueId FPoseSearchDatabaseAsyncCacheTask::Id = UE::DerivedData::FValueId::FromName("Data");
	const UE::DerivedData::FCacheBucket FPoseSearchDatabaseAsyncCacheTask::Bucket("PoseSearchDatabase");
}
#endif // WITH_EDITOR

#if WITH_EDITOR

void FPoseSearchDatabaseDerivedData::Cache(UPoseSearchDatabase& Database, bool bForceRebuild)
{
	check(IsInGameThread());

	CancelCache();

#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
	UE_LOG(LogPoseSearch, Log, TEXT("%s: Cache"), *LexToString(PendingDerivedDataKey));
#endif // UE_POSE_SEARCH_DERIVED_DATA_LOGGING

	if (Database.IsValidForIndexing())
	{
		CreateDatabaseBuildTask(Database, bForceRebuild);
	}
	else
	{
		SearchIndex.Reset();
		SearchIndex.Schema = Database.Schema;
		DerivedDataKey = { UE::DerivedData::FCacheBucket(), FIoHash::Zero };
		PendingDerivedDataKey = FIoHash::Zero;
	}
}

void FPoseSearchDatabaseDerivedData::CancelCache()
{
	check(IsInGameThread());

	if (AsyncTask)
	{
#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		UE_LOG(LogPoseSearch, Log, TEXT("%s: CancelCache"), *LexToString(PendingDerivedDataKey));
#endif // UE_POSE_SEARCH_DERIVED_DATA_LOGGING

		AsyncTask->Cancel();
	}

	FinishCache();
}

void FPoseSearchDatabaseDerivedData::FinishCache()
{
	check(IsInGameThread());

	if (AsyncTask)
	{
#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		UE_LOG(LogPoseSearch, Log, TEXT("%s: FinishCache"), *LexToString(PendingDerivedDataKey));
#endif // UE_POSE_SEARCH_DERIVED_DATA_LOGGING

		AsyncTask->Wait();
		delete AsyncTask;
		AsyncTask = nullptr;
	}
}

void FPoseSearchDatabaseDerivedData::CreateDatabaseBuildTask(UPoseSearchDatabase& Database, bool bForceRebuild)
{
	check(IsInGameThread());

	AsyncTask = new UE::PoseSearch::FPoseSearchDatabaseAsyncCacheTask(Database, *this, bForceRebuild);
}

#endif // WITH_EDITOR

namespace UE::PoseSearch
{

#if WITH_EDITOR

	FPoseSearchDatabaseAsyncCacheTask::FPoseSearchDatabaseAsyncCacheTask(
		UPoseSearchDatabase& InDatabase,
		FPoseSearchDatabaseDerivedData& InDerivedData,
		bool bForceRebuild)
		: Owner(UE::DerivedData::EPriority::Normal)
		, DerivedData(InDerivedData)
		, Database(InDatabase)
	{
		using namespace UE::DerivedData;

		FIoHash DerivedDataKey = CreateKey(Database);
		DerivedData.PendingDerivedDataKey = DerivedDataKey;

		InDatabase.NotifyDerivedDataBuildStarted();

		if (bForceRebuild)
		{
			// when the build is forced, the derived data key is zeroed so the comparison with the pending key fails, 
			// informing other systems that data is being rebuilt
			DerivedData.DerivedDataKey.Hash = FIoHash::Zero;
			BuildAndWrite({ Bucket, DerivedDataKey });
		}
		else
		{
			BeginCache();
		}
	}

	bool FPoseSearchDatabaseAsyncCacheTask::Cancel()
	{
		Owner.Cancel();
		return true;
	}

	void FPoseSearchDatabaseAsyncCacheTask::Wait()
	{
		Owner.Wait();
	}

	bool FPoseSearchDatabaseAsyncCacheTask::Poll() const
	{
		return Owner.Poll();
	}


	void FPoseSearchDatabaseAsyncCacheTask::BeginCache()
	{
		using namespace UE::DerivedData;

#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		UE_LOG(LogPoseSearch, Log, TEXT("%s: BeginCache - '%s'"), *LexToString(DerivedData.PendingDerivedDataKey), *Database.GetName());
#endif // UE_POSE_SEARCH_DERIVED_DATA_LOGGING

		TArray<FCacheGetRequest> CacheRequests;
		ECachePolicy CachePolicy = ECachePolicy::Default;
		const FCacheKey CacheKey{ Bucket, DerivedData.PendingDerivedDataKey };
		CacheRequests.Add({ {Database.GetPathName()}, CacheKey, CachePolicy });
		GetCache().Get(CacheRequests, Owner, [this](FCacheGetResponse&& Response)
		{
			OnGetComplete(MoveTemp(Response));
		});
	}

	void FPoseSearchDatabaseAsyncCacheTask::OnGetComplete(UE::DerivedData::FCacheGetResponse&& Response)
	{
		using namespace UE::DerivedData;

		if (Response.Status == EStatus::Ok)
		{
			BuildIndexFromCacheRecord(MoveTemp(Response.Record));
			DerivedData.DerivedDataKey = Response.Record.GetKey();
		}
		else if (Response.Status == EStatus::Error)
		{
			BuildAndWrite(Response.Record.GetKey());
		}
	}

	void FPoseSearchDatabaseAsyncCacheTask::BuildAndWrite(const UE::DerivedData::FCacheKey& NewKey)
	{
		GetRequestOwner().LaunchTask(TEXT("PoseSearchDatabaseBuild"), [this, NewKey]
		{
			if (!GetRequestOwner().IsCanceled())
			{
				DerivedData.SearchIndex.Reset();
				DerivedData.SearchIndex.Schema = Database.Schema;
				const bool bIndexReady = BuildIndex(&Database, DerivedData.SearchIndex);

				WriteIndexToCache(NewKey);

#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
				UE_LOG(LogPoseSearch, Log, TEXT("%s: BuildAndWrite"), *LexToString(DerivedData.PendingDerivedDataKey));
#endif // UE_POSE_SEARCH_DERIVED_DATA_LOGGING
			}
		});
	}

	void FPoseSearchDatabaseAsyncCacheTask::WriteIndexToCache(const UE::DerivedData::FCacheKey& NewKey)
	{
		using namespace UE::DerivedData;

		TArray<uint8> RawBytes;
		FMemoryWriter Writer(RawBytes);
		Writer << DerivedData.SearchIndex;
		FSharedBuffer RawData = MakeSharedBufferFromArray(MoveTemp(RawBytes));

		FCacheRecordBuilder Builder(NewKey);
		Builder.AddValue(Id, RawData);

		Owner.KeepAlive();
		GetCache().Put({ { { Database.GetPathName() }, Builder.Build() } }, Owner);
		DerivedData.DerivedDataKey = NewKey;
	}

	void FPoseSearchDatabaseAsyncCacheTask::BuildIndexFromCacheRecord(UE::DerivedData::FCacheRecord&& CacheRecord)
	{
		using namespace UE::DerivedData;

#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		UE_LOG(LogPoseSearch, Log, TEXT("%s: BuildIndexFromCacheRecord"), *LexToString(DerivedData.PendingDerivedDataKey));
#endif // UE_POSE_SEARCH_DERIVED_DATA_LOGGING

		DerivedData.SearchIndex.Reset();
		DerivedData.SearchIndex.Schema = Database.Schema;

		FSharedBuffer RawData = CacheRecord.GetValue(Id).GetData().Decompress();
		FMemoryReaderView Reader(RawData);
		Reader << DerivedData.SearchIndex;
	}

	FIoHash FPoseSearchDatabaseAsyncCacheTask::CreateKey(UPoseSearchDatabase& Database)
	{
		using UE::PoseSearch::FDerivedDataKeyBuilder;

#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		const double StartTime = FPlatformTime::Seconds();
#endif
		FDerivedDataKeyBuilder KeyBuilder;
		FGuid VersionGuid = FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().POSESEARCHDB_DERIVEDDATA_VER);
		KeyBuilder << VersionGuid;
		Database.BuildDerivedDataKey(KeyBuilder);
		FDerivedDataKeyBuilder::HashDigestType Hash = KeyBuilder.Finalize();

		// Stores a BLAKE3-160 hash, taken from the first 20 bytes of a BLAKE3-256 hash
		FIoHash IoHash(Hash);

#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		const double TotalTime = FPlatformTime::Seconds() - StartTime;
		UE_LOG(LogPoseSearch, Log, TEXT("%s: CreateKey ('%s' - %.0f µs)"), *LexToString(IoHash), *LexToString(Hash), *Database.GetName(), TotalTime * 1e6);
#endif // UE_POSE_SEARCH_DERIVED_DATA_LOGGING

		return IoHash;
	}

#endif // WITH_EDITOR

	FArchive& operator<<(FArchive& Ar, FPoseSearchIndex& Index)
	{
		int32 NumValues = 0;
		int32 NumPCAValues = 0;
		int32 NumAssets = 0;

		if (Ar.IsSaving())
		{
			NumValues = Index.Values.Num();
			NumPCAValues = Index.PCAValues.Num();
			NumAssets = Index.Assets.Num();
		}

		Ar << Index.NumPoses;
		Ar << NumValues;
		Ar << NumPCAValues;
		Ar << NumAssets;

		if (Ar.IsLoading())
		{
			Index.Values.SetNumUninitialized(NumValues);
			Index.PCAValues.SetNumUninitialized(NumPCAValues);
			Index.PoseMetadata.SetNumUninitialized(Index.NumPoses);
			Index.Assets.SetNumUninitialized(NumAssets);
		}

		if (Index.Values.Num() > 0)
		{
			Ar.Serialize(&Index.Values[0], Index.Values.Num() * Index.Values.GetTypeSize());
		}

		if (Index.PCAValues.Num() > 0)
		{
			Ar.Serialize(&Index.PCAValues[0], Index.PCAValues.Num() * Index.PCAValues.GetTypeSize());
		}

		if (Index.PoseMetadata.Num() > 0)
		{
			Ar.Serialize(&Index.PoseMetadata[0], Index.PoseMetadata.Num() * Index.PoseMetadata.GetTypeSize());
		}

		if (Index.Assets.Num() > 0)
		{
			Ar.Serialize(&Index.Assets[0], Index.Assets.Num() * Index.Assets.GetTypeSize());
		}

		Ar << Index.WeightsSqrt;
		Ar << Index.Mean;
		Ar << Index.PCAProjectionMatrix;

		Serialize(Ar, Index.KDTree, Index.PCAValues.GetData());

		return Ar;
	}

}