// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Engine/World.h"
#include "IAnimationBudgetAllocatorModule.h"
#include "Engine/World.h"

class FAnimationBudgetAllocator;

class ANIMATIONBUDGETALLOCATOR_API FAnimationBudgetAllocatorModule : public IAnimationBudgetAllocatorModule, public FGCObject
{
public:
	// IAnimationBudgetAllocatorModule interface
	virtual IAnimationBudgetAllocator* GetBudgetAllocatorForWorld(UWorld* World) override;

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FAnimationBudgetAllocatorModule");
	}

private:
	/** Handle world initialization */
	void HandleWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

	/** Handle world cleanup */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	/** Delegate handles for hooking into UWorld lifetimes */
	FDelegateHandle PreWorldInitializationHandle;
	FDelegateHandle PostWorldCleanupHandle;

	/** Map of world->budgeter */
	TMap<UWorld*, FAnimationBudgetAllocator*> WorldAnimationBudgetAllocators;
};
