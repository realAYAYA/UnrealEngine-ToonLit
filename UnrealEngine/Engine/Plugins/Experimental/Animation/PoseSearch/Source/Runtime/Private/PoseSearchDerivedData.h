// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearch.h"

#if WITH_EDITORONLY_DATA

#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"

#endif // WITH_EDITORONLY_DATA

#include "PoseSearchDerivedData.generated.h"

namespace UE::DerivedData
{
	struct FCacheGetResponse;
	class FCacheRecord;
}

USTRUCT()
struct FPoseSearchDatabaseDerivedData
{
	GENERATED_BODY()

	UPROPERTY()
	FPoseSearchIndex SearchIndex;

#if WITH_EDITORONLY_DATA
	UE::DerivedData::FCacheKey DerivedDataKey;


	/** This is the key for the FetchOrBuild variant of our Cache. We assume that uniqueness
	*	for that is equivalent to uniqueness if we use both FetchFirst and FetchOrBuild. This
	*	is used as the key in to CookedPlatformData, as well as to determine if we are already
	*	cooking the data the editor needs in CachePlatformData. 
	*	Note that since this is read on the game thread constantly in CachePlatformData, it
	*	must be written to on the game thread to avoid false recaches.
	*/
	FIoHash PendingDerivedDataKey;

	/** Async cache task if one is outstanding. */
	UE::PoseSearch::FPoseSearchDatabaseAsyncCacheTask* AsyncTask;


#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	// Build search index and adds result to DDC
	void Cache(UPoseSearchDatabase& Database, bool bForceRebuild);

	void CancelCache();

	void FinishCache();

	void CreateDatabaseBuildTask(UPoseSearchDatabase& Database, bool bForceRebuild);
#endif // WITH_EDITOR
};

namespace UE::PoseSearch
{
#if WITH_EDITOR
	struct FPoseSearchDatabaseAsyncCacheTask
	{

	public:
		FPoseSearchDatabaseAsyncCacheTask(
			UPoseSearchDatabase& InDatabase,
			FPoseSearchDatabaseDerivedData& InDerivedData,
			bool bForceRebuild);

		bool Cancel();
		void Wait();
		bool Poll() const;

	private:
		void BeginCache();
		void OnGetComplete(UE::DerivedData::FCacheGetResponse&& Response);
		void BuildAndWrite(const UE::DerivedData::FCacheKey& NewKey);
		void WriteIndexToCache(const UE::DerivedData::FCacheKey& NewKey);
		void BuildIndexFromCacheRecord(UE::DerivedData::FCacheRecord&& CacheRecord);

		UE::DerivedData::IRequestOwner& GetRequestOwner() { return Owner; }

	private:
		UE::DerivedData::FRequestOwner Owner;
		FPoseSearchDatabaseDerivedData& DerivedData;
		UPoseSearchDatabase& Database;

		static const UE::DerivedData::FValueId Id;
		static const UE::DerivedData::FCacheBucket Bucket;

	public:
		static FIoHash CreateKey(UPoseSearchDatabase& Database);
	};
#endif // WITH_EDITOR

	// Serialization for FPoseSearchIndex.
	FArchive& operator<<(FArchive& Ar, FPoseSearchIndex& Index);
}