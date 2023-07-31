// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#include "Containers/Set.h"
#include "CoreTypes.h"
#include "NaniteDisplacedMesh.h"
#include "UObject/GCObject.h"
#include "UObject/WeakObjectPtr.h"

class UNaniteDisplacedMesh;
class FQueuedThreadPool;
struct FAssetCompileContext;
enum class EQueuedWorkPriority : uint8;

class FNaniteDisplacedMeshCompilingManager : public IAssetCompilingManager, public FGCObject
{
public:
	static FNaniteDisplacedMeshCompilingManager& Get();

	/**
	 * Returns the number of outstanding compilations.
	 */
	int32 GetNumRemainingAssets() const override;

	/**
	 * Queue nanite displaced meshes to be compiled asynchronously so they are monitored.
	 */
	void AddNaniteDisplacedMeshes(TArrayView<UNaniteDisplacedMesh* const> InNaniteDisplacedMeshes);

	/**
	 * Blocks until completion of the requested nanite displaced meshes.
	 */
	void FinishCompilation(TArrayView<UNaniteDisplacedMesh* const> InNaniteDisplacedMeshes);

	/**
	 * Blocks until completion of all async nanite displaced mesh compilation.
	 */
	void FinishAllCompilation() override;

	/**
	 * Returns the priority at which the given nanite displaced mesh should be scheduled.
	 */
	EQueuedWorkPriority GetBasePriority(UNaniteDisplacedMesh* InNaniteDisplacedMeshes) const;

	/**
	 * Returns the thread pool where nanite displaced mesh compilation should be scheduled.
	 */
	FQueuedThreadPool* GetThreadPool() const;

	/**
	 * Cancel any pending work and blocks until it is safe to shut down.
	 */
	void Shutdown() override;

	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject interface

private:
	friend class FAssetCompilingManager;

	FNaniteDisplacedMeshCompilingManager();

	FName GetAssetTypeName() const override;
	FTextFormat GetAssetNameFormat() const override;
	TArrayView<FName> GetDependentTypeNames() const override;

	/** Called once per frame, fetches completed tasks and applies them to the scene. */
	void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;

	bool bHasShutdown = false;
	TSet<TWeakObjectPtr<UNaniteDisplacedMesh>> RegisteredNaniteDisplacedMesh;

	// Refer the transient displaced meshes to the GC so that they are not GCed while compiling. Without this a pie session can hitches a lot when the ddc is not primed.
	TSet<UNaniteDisplacedMesh*> GCReferedNaniteDisplacedMesh;

	FAsyncCompilationNotification Notification;

	void FinishCompilationsForGame();
	void Reschedule();
	void ProcessNaniteDisplacedMeshes(bool bLimitExecutionTime, int32 MinBatchSize = 1);
	void UpdateCompilationNotification();

	void PostCompilation(TArrayView<UNaniteDisplacedMesh* const> InNaniteDisplacedMeshes);
	void PostCompilation(UNaniteDisplacedMesh* InNaniteDisplacedMesh);

	void OnPostReachabilityAnalysis();
	FDelegateHandle PostReachabilityAnalysisHandle;
};

#endif // WITH_EDITOR