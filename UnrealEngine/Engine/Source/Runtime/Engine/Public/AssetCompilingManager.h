// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"
#include "Interfaces/Interface_AsyncCompilation.h"
#include "AssetCompilingManager.generated.h"

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

struct IAssetCompilingManager
{
	/**
	 * A unique name among all asset compiling manager to identify the type of asset this manager handles.
	 */
	ENGINE_API virtual FName GetAssetTypeName() const = 0;

	/**
	 * Returns an FTextFormat representing a localized singular/plural formatter for this resource name.
	 * 
	 * Note: Should be in a similar form as "{0}|plural(one=Singular Name,other=Plural Name)"
	 */
	ENGINE_API virtual FTextFormat GetAssetNameFormat() const = 0;

	/**
	 * Return other asset types that should preferably be handled before this one.
	 */
	ENGINE_API virtual TArrayView<FName> GetDependentTypeNames() const = 0;

	/**
	 * Returns the number of remaining compilations.
	 */
	ENGINE_API virtual int32 GetNumRemainingAssets() const = 0;

	/**
	 * Blocks until completion of all assets.
	 */
	ENGINE_API virtual void FinishAllCompilation() = 0;

	/**
	 * Cancel any pending work and blocks until it is safe to shut down.
	 */
	ENGINE_API virtual void Shutdown() = 0;

protected:
	friend class FAssetCompilingManager;

	/** 
	 * Called once per frame, fetches completed tasks and applies them to the scene. 
	 */
	ENGINE_API virtual void ProcessAsyncTasks(bool bLimitExecutionTime = false) = 0;


	ENGINE_API virtual ~IAssetCompilingManager() {}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FAssetPostCompileEvent, const TArray<FAssetCompileData>&);

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
	 * Event called after an asset finishes compilation.
	 */
	FAssetPostCompileEvent& OnAssetPostCompileEvent() { return AssetPostCompileEvent; }
private:
	FAssetCompilingManager();
	friend struct IAssetCompilingManager;

	/** Take some action whenever the number of remaining asset changes. */
	void UpdateNumRemainingAssets();

	bool bHasShutdown = false;
	int32 LastNumRemainingAssets = 0;

	TArray<IAssetCompilingManager*> AssetCompilingManagers;
	TArray<IAssetCompilingManager*> AssetCompilingManagersWithValidDependencies;

	/** Event issued at the end of the compile process */
	FAssetPostCompileEvent AssetPostCompileEvent;
};
