// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Modules/ModuleInterface.h"

class UContextualAnimManager;

class FContextualAnimationModule : public IModuleInterface, public FGCObject
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Begin FGCObject overrides
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FContextualAnimationModule");
	}
	// End FGCObject overrides

	FORCEINLINE static UContextualAnimManager* GetManager(const UWorld* World)
	{
		return WorldToManagerMap.FindRef(World);
	}

private:

	static void OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	static TMap<const UWorld*, UContextualAnimManager*> WorldToManagerMap;

	FDelegateHandle OnPreWorldInitDelegateHandle;
	FDelegateHandle OnPostWorldCleanupDelegateHandle;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#endif
