// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavAreas/NavArea.h"
#include "GameFramework/Volume.h"
#include "NavModifierVolume.generated.h"

struct FNavigationRelevantData;

/** 
 *	Allows applying selected AreaClass to navmesh, using Volume's shape
 */
UCLASS(hidecategories=(Navigation))
class NAVIGATIONSYSTEM_API ANavModifierVolume : public AVolume, public INavRelevantInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Default)
	TSubclassOf<UNavArea> AreaClass;

	/** Experimental: if set, the 2D space occupied by the volume box will ignore FillCollisionUnderneathForNavmesh */
	UPROPERTY(EditAnywhere, Category = Default, AdvancedDisplay)
	bool bMaskFillCollisionUnderneathForNavmesh;

	/** Experimental: When not set to None, the navmesh tiles touched by the navigation modifier volume will be built
	 * using the highest resolution found. */
	UPROPERTY(EditAnywhere, Category = Default, AdvancedDisplay)
	ENavigationDataResolution NavMeshResolution;

	FDelegateHandle OnNavAreaRegisteredDelegateHandle;
	FDelegateHandle OnNavAreaUnregisteredDelegateHandle;

public:
	ANavModifierVolume(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void SetAreaClass(TSubclassOf<UNavArea> NewAreaClass = nullptr);

	TSubclassOf<UNavArea> GetAreaClass() const { return AreaClass; }

	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual FBox GetNavigationBounds() const override;
	virtual void RebuildNavigationData() override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;

	void OnNavAreaRegistered(const UWorld& World, const UClass* NavAreaClass);
	void OnNavAreaUnregistered(const UWorld& World, const UClass* NavAreaClass);
};
