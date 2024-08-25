// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/WeakObjectPtr.h"
#include "Containers/Set.h"
#include "IAssetCompilingManager.h"

#if WITH_EDITOR

class FAsyncCompilationNotification;
class UStaticMesh;
class UPrimitiveComponent;
class UStaticMeshComponent;
class FQueuedThreadPool;
struct FAssetCompileContext;
enum class EQueuedWorkPriority : uint8;

class FStaticMeshCompilingManager : IAssetCompilingManager
{
public:
	ENGINE_API static FStaticMeshCompilingManager& Get();

	/**
	 * Returns true if the feature is currently activated.
	 */
	ENGINE_API bool IsAsyncStaticMeshCompilationEnabled() const;

	/** 
	 * Returns the number of outstanding texture compilations.
	 */
	ENGINE_API int32 GetNumRemainingMeshes() const;

	/** 
	 * Adds static meshes compiled asynchronously so they are monitored. 
	 */
	ENGINE_API void AddStaticMeshes(TArrayView<UStaticMesh* const> InStaticMeshes);

	/** 
	 * Blocks until completion of the requested static meshes.
	 */
	ENGINE_API void FinishCompilation(TArrayView<UStaticMesh* const> InStaticMeshes);

	/** 
	 * Blocks until completion of all async static mesh compilation.
	 */
	ENGINE_API void FinishAllCompilation() override;

	/**
	 * Returns if asynchronous compilation is allowed for this static mesh.
	 */
	ENGINE_API bool IsAsyncCompilationAllowed(UStaticMesh* InStaticMesh) const;

	/**
	 * Returns the priority at which the given static mesh should be scheduled.
	 */
	ENGINE_API EQueuedWorkPriority GetBasePriority(UStaticMesh* InStaticMesh) const;

	/**
	 * Returns the threadpool where static mesh compilation should be scheduled.
	 */
	ENGINE_API FQueuedThreadPool* GetThreadPool() const;

	/**
	 * Cancel any pending work and blocks until it is safe to shut down.
	 */
	ENGINE_API void Shutdown() override;

	/** Get the name of the asset type this compiler handles */
	ENGINE_API static FName GetStaticAssetTypeName();

private:
	friend class FAssetCompilingManager;

	FStaticMeshCompilingManager();

	FName GetAssetTypeName() const override;
	FTextFormat GetAssetNameFormat() const override;
	TArrayView<FName> GetDependentTypeNames() const override;
	int32 GetNumRemainingAssets() const override;

	/** Called once per frame, fetches completed tasks and applies them to the scene. */
	void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;

	/** Handle generic finish compilation */
	void FinishCompilationForObjects(TArrayView<UObject* const> InObjects) override;

	bool bHasShutdown = false;
	TSet<TWeakObjectPtr<UStaticMesh>> RegisteredStaticMesh;
	TUniquePtr<FAsyncCompilationNotification> Notification;

	void FinishCompilationsForGame();
	void Reschedule();
	void ProcessStaticMeshes(bool bLimitExecutionTime, int32 MinBatchSize = 1);
	void UpdateCompilationNotification();

	void PostCompilation(TArrayView<UStaticMesh* const> InStaticMeshes);
	void PostCompilation(UStaticMesh* StaticMesh);

	void OnPostReachabilityAnalysis();
	FDelegateHandle PostReachabilityAnalysisHandle;
};

#endif // #if WITH_EDITOR

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#endif
