// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"
#include "Interfaces/Interface_AsyncCompilation.h"
#include "AssetCompilingManager.generated.h"

class FQueuedThreadPool;

USTRUCT(BlueprintType)
struct FAssetCompileData
{
	GENERATED_BODY();

	/** Object being compiled */
	UPROPERTY(BlueprintReadWrite, Category = AssetCompileData)
	TWeakObjectPtr<UObject> Asset;

	FAssetCompileData()
	{}

	FAssetCompileData(const TWeakObjectPtr<UObject>& InAsset)
		: Asset(InAsset)
	{
	}
};

namespace AssetCompilation
{
	struct FProcessAsyncTaskParams
	{
		/* Limits the execution time instead of processing everything */
		bool bLimitExecutionTime = false;

		/* Limits processing for assets required for PIE only. */
		bool bPlayInEditorAssetsOnly = false;
	};
}

struct IAssetCompilingManager
{
	/**
	 * A unique name among all asset compiling manager to identify the type of asset this manager handles.
	 */
	virtual FName GetAssetTypeName() const = 0;

	/**
	 * Returns an FTextFormat representing a localized singular/plural formatter for this resource name.
	 * 
	 * Note: Should be in a similar form as "{0}|plural(one=Singular Name,other=Plural Name)"
	 */
	virtual FTextFormat GetAssetNameFormat() const = 0;

	/**
	 * Return other asset types that should preferably be handled before this one.
	 */
	virtual TArrayView<FName> GetDependentTypeNames() const = 0;

	/**
	 * Returns the number of remaining compilations.
	 */
	virtual int32 GetNumRemainingAssets() const = 0;

	/**
	 * Blocks until completion of the requested objects.
	 */
	virtual void FinishCompilationForObjects(TArrayView<UObject* const> InObjects) {} /* Optional for backward compatibility */

	/**
	 * Blocks until completion of all assets.
	 */
	virtual void FinishAllCompilation() = 0;

	/**
	 * Cancel any pending work and blocks until it is safe to shut down.
	 */
	virtual void Shutdown() = 0;

protected:
	friend class FAssetCompilingManager;

	/** 
	 * Called once per frame, fetches completed tasks and applies them to the scene. 
	 */
	virtual void ProcessAsyncTasks(bool bLimitExecutionTime = false) = 0;

	/**
	 * Called once per frame, fetches completed tasks and applies them to the scene.
	 */
	virtual void ProcessAsyncTasks(const AssetCompilation::FProcessAsyncTaskParams& Params)
	{
		/* Forward for backward compatibility */
		ProcessAsyncTasks(Params.bLimitExecutionTime);
	}

	virtual ~IAssetCompilingManager() {}
};

DECLARE_TS_MULTICAST_DELEGATE_OneParam(FAssetPostCompileEvent, const TArray<FAssetCompileData>&);

class FAssetCompilingManager
{
public:
	ENGINE_API static FAssetCompilingManager& Get();

	/**
	 * Register an asset compiling manager.
	 */
	ENGINE_API bool RegisterManager(IAssetCompilingManager* InAssetCompilingManager);

	/**
	 * Unregister an asset compiling manager.
	 */
	ENGINE_API bool UnregisterManager(IAssetCompilingManager* InAssetCompilingManager);

	/**
	 * Register the list of registered managers.
	 */
	ENGINE_API TArrayView<IAssetCompilingManager* const> GetRegisteredManagers() const;

	/** 
	 * Returns the number of remaining compilations.
	 */
	ENGINE_API int32 GetNumRemainingAssets() const;

	/** 
	 * Blocks until completion of all assets.
	 */
	ENGINE_API void FinishAllCompilation();

	/**
	 * Finish compilation of the requested objects.
	 */
	ENGINE_API void FinishCompilationForObjects(TArrayView<UObject* const> InObjects);

	/**
	 * Cancel any pending work and blocks until it is safe to shut down.
	 */
	ENGINE_API void Shutdown();

	/**
	 * Returns the thread-pool where asset compilation should be scheduled.
	 */
	ENGINE_API FQueuedThreadPool* GetThreadPool() const;

	/** 
	 * Called once per frame, fetches completed tasks and applies them to the scene. 
	 */
	ENGINE_API void ProcessAsyncTasks(bool bLimitExecutionTime = false);

	/**
	 * Called once per frame, fetches completed tasks and applies them to the scene.
	 */
	ENGINE_API void ProcessAsyncTasks(const AssetCompilation::FProcessAsyncTaskParams& Params);

	/**
	 * Event called after an asset finishes compilation.
	 */
	FAssetPostCompileEvent& OnAssetPostCompileEvent() { return AssetPostCompileEvent; }

	/**
	 * Event called before and after FinishAllCompilation or ProcessAsyncTasks run operations on a specific package,
	 * used for subscribers to associate low-level actions with that Package (e.g. TObjectPtr reads).
	 * void OnPackageScopeEvent(UPackage* Package, bool bEntering)
	 * Called with bEntering=true followed by actions for Package followed by call with bEntering=false.
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FPackageScopeEvent, UPackage*, bool);
	FPackageScopeEvent& OnPackageScopeEvent() { return PackageScopeEvent; }

private:
	FAssetCompilingManager();
	~FAssetCompilingManager();
	friend struct IAssetCompilingManager;

	/** Take some action whenever the number of remaining asset changes. */
	void UpdateNumRemainingAssets();

	bool bHasShutdown = false;
	int32 LastNumRemainingAssets = 0;

	TArray<IAssetCompilingManager*> AssetCompilingManagers;
	TArray<IAssetCompilingManager*> AssetCompilingManagersWithValidDependencies;

	/** Event issued at the end of the compile process */
	FAssetPostCompileEvent AssetPostCompileEvent;

	FPackageScopeEvent PackageScopeEvent;
};
