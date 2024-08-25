// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/WeakObjectPtr.h"
#include "Containers/Set.h"
#include "Tickable.h"
#include "Stats/Stats.h"
#include "IAssetCompilingManager.h"

#if WITH_EDITOR

class FAsyncCompilationNotification;
class USkinnedAsset;
class UPrimitiveComponent;
class FQueuedThreadPool;
struct FAssetCompileContext;
enum class EQueuedWorkPriority : uint8;

class FSkinnedAssetCompilingManager : IAssetCompilingManager
{
public:
	ENGINE_API static FSkinnedAssetCompilingManager& Get();

	/**
	 * Returns true if the feature is currently activated.
	 */
	ENGINE_API bool IsAsyncCompilationEnabled() const;

	/** 
	 * Returns the number of outstanding compilations.
	 */
	ENGINE_API int32 GetNumRemainingJobs() const;

	/**
	 * Adds skeletal meshes compiled asynchronously so they are monitored.
	 */
	UE_DEPRECATED(5.1, "Use AddSkinnedAssets instead.")
	ENGINE_API void AddSkeletalMeshes(TArrayView<class USkeletalMesh* const> InSkeletalMeshes);

	/** 
	 * Adds skinned assets compiled asynchronously so they are monitored. 
	 */
	ENGINE_API void AddSkinnedAssets(TArrayView<USkinnedAsset* const> InSkinnedAssets);

	/** 
	 * Blocks until completion of the requested skinned assets.
	 */
	ENGINE_API void FinishCompilation(TArrayView<USkinnedAsset* const> InSkinnedAssets);

	/** 
	 * Blocks until completion of all async skinned asset compilation.
	 */
	ENGINE_API void FinishAllCompilation() override;

	/**
	 * Returns if asynchronous compilation is allowed for this skinned asset.
	 */
	ENGINE_API bool IsAsyncCompilationAllowed(USkinnedAsset* InSkinnedAsset) const;

	/**
	 * Returns the priority at which the given skinned asset should be scheduled.
	 */
	ENGINE_API EQueuedWorkPriority GetBasePriority(USkinnedAsset* InSkinnedAsset) const;

	/**
	 * Returns the threadpool where skinned asset compilation should be scheduled.
	 */
	ENGINE_API FQueuedThreadPool* GetThreadPool() const;

	/**
	 * Cancel any pending work and blocks until it is safe to shut down.
	 */
	ENGINE_API void Shutdown() override;

private:
	FSkinnedAssetCompilingManager();

	FName GetAssetTypeName() const override;
	FTextFormat GetAssetNameFormat() const override;
	TArrayView<FName> GetDependentTypeNames() const override;
	int32 GetNumRemainingAssets() const override;
	void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;

	void FinishCompilationForObjects(TArrayView<UObject* const> InObjects) override;

	friend class FAssetCompilingManager;
	
	bool bHasShutdown = false;
	TSet<TWeakObjectPtr<USkinnedAsset>> RegisteredSkinnedAsset;
	TUniquePtr<FAsyncCompilationNotification> Notification;
	void FinishCompilationsForGame();
	void Reschedule();
	void ProcessSkinnedAssets(bool bLimitExecutionTime, int32 MinBatchSize = 1);
	void UpdateCompilationNotification();

	void PostCompilation(USkinnedAsset* SkinnedAsset);
	void PostCompilation(TArrayView<USkinnedAsset* const> InSkinnedAssets);

	void OnPostReachabilityAnalysis();
	FDelegateHandle PostReachabilityAnalysisHandle;
	
	void OnPreGarbageCollect();
	FDelegateHandle PreGarbageCollectHandle;
};

#endif // #if WITH_EDITOR

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#endif
