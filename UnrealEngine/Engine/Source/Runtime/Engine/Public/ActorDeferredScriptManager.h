// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/WeakObjectPtr.h"
#include "Containers/Deque.h"
#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"

class AActor;

#if WITH_EDITOR

class FActorDeferredScriptManager : IAssetCompilingManager
{
public:
	ENGINE_API static FActorDeferredScriptManager& Get();

	/**
	 * Cancel any pending work and blocks until it is safe to shut down.
	 */
	ENGINE_API void Shutdown() override;

	/** Get the name of the asset type this compiler handles */
	ENGINE_API static FName GetStaticAssetTypeName();

	/**
	 * empty implementation since deferred actor scripts are not actually async
	 */
	ENGINE_API void FinishAllCompilation() override {}


	/** 
	 * Add an actor for the manager to run the construction of once dependent asset compilation are done (i.e Static Mesh)
	 */
	void AddActor(AActor* InActor);

private:
	friend class FAssetCompilingManager;

	FActorDeferredScriptManager();

	FName GetAssetTypeName() const override;
	FTextFormat GetAssetNameFormat() const override;
	TArrayView<FName> GetDependentTypeNames() const override;
	int32 GetNumRemainingAssets() const override;
	void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;

	void UpdateCompilationNotification();

	/** Actors awaiting outstanding asset compilation prior to running their non trivial construction scripts after loading this level. */
	TDeque<TWeakObjectPtr<AActor>> PendingConstructionScriptActors;

	/** Notification for the amount of pending construction scripts to run */
	FAsyncCompilationNotification Notification;
};

#endif