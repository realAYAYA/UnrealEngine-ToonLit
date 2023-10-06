// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "TickableEditorObject.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectSaveContext.h"

class UPoseSearchDatabase;

namespace UE::PoseSearch
{
	struct FPoseSearchDatabaseAsyncCacheTask;
	class FPoseSearchDatabaseAsyncCacheTasks;

	enum class ERequestAsyncBuildFlag
	{
		NewRequest = 1 << 0,			// generates new key and kick off a task to get updated data (it'll cancel for eventual previous Database request, unless WaitPreviousRequest)
		ContinueRequest = 1 << 1,		// make sure there's associated data to the Database (doesn't have to be up to date)
		WaitForCompletion = 1 << 2,		// wait the termination of the NewRequest or ContinueRequest
	};
	ENUM_CLASS_FLAGS(ERequestAsyncBuildFlag);

	class POSESEARCH_API FAsyncPoseSearchDatabasesManagement : public FTickableEditorObject, public FTickableCookObject, public FGCObject
	{
	public:
		static bool RequestAsyncBuildIndex(const UPoseSearchDatabase* Database, ERequestAsyncBuildFlag Flag);

	private:
		FAsyncPoseSearchDatabasesManagement();
		~FAsyncPoseSearchDatabasesManagement();

		static FAsyncPoseSearchDatabasesManagement& Get();

		void OnObjectModified(UObject* Object);
		void OnPackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent);

		void Shutdown();
		void StartQueuedTasks(int32 MaxActiveTasks);

		// Begin FTickableEditorObject
		virtual void Tick(float DeltaTime) override;
		virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
		virtual TStatId GetStatId() const override;
		// End FTickableEditorObject

		// Begin FTickableCookObject
		virtual void TickCook(float DeltaTime, bool bCookCompete) override;
		// End FTickableCookObject

		// Begin FGCObject
		void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FAsyncPoseSearchDatabaseManagement"); }
		// End FGCObject
		
		FPoseSearchDatabaseAsyncCacheTasks& Tasks;
		FDelegateHandle OnObjectModifiedHandle;
		FDelegateHandle OnPackageReloadedHandle;
		
		static FCriticalSection Mutex;
	};
} // namespace UE::PoseSearch

#endif // WITH_EDITOR