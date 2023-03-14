// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "NavRelevantComponent.generated.h"

UCLASS()
class NAVIGATIONSYSTEM_API UNavRelevantComponent : public UActorComponent, public INavRelevantInterface
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~ End UActorComponent Interface

	//~ Begin INavRelevantInterface Interface
	virtual FBox GetNavigationBounds() const override;
	virtual bool IsNavigationRelevant() const override;
	virtual void UpdateNavigationBounds() override;
	virtual UObject* GetNavigationParent() const override;
	//~ End INavRelevantInterface Interface
	
	virtual void CalcAndCacheBounds() const;

	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	void SetNavigationRelevancy(bool bRelevant);

	/** force relevancy and skip attaching navigation data to owner's root entry */
	void ForceNavigationRelevancy(bool bForce);

	/** force refresh in navigation octree */
	void RefreshNavigationModifiers();
	
protected:

	/** bounds for navigation octree */
	mutable FBox Bounds;
	
	/** attach navigation data to entry for owner's root component (depends on its relevancy) */
	UPROPERTY()
	uint32 bAttachToOwnersRoot : 1;

	mutable uint32 bBoundsInitialized : 1;
	uint32 bNavParentCacheInitialized : 1;

	UPROPERTY(transient)
	TObjectPtr<UObject> CachedNavParent;
};
