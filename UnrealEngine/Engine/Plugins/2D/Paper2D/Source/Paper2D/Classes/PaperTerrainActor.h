// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "PaperTerrainActor.generated.h"

class UPaperTerrainComponent;
class UPaperTerrainSplineComponent;

/**
 * An instance of a piece of 2D terrain in the level
 */

UCLASS(BlueprintType, Experimental)
class PAPER2D_API APaperTerrainActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY()
	TObjectPtr<class USceneComponent> DummyRoot;

	UPROPERTY()
	TObjectPtr<class UPaperTerrainSplineComponent> SplineComponent;

	UPROPERTY(Category=Sprite, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories="Sprite,Rendering,Physics,Components|Sprite", AllowPrivateAccess="true"))
	TObjectPtr<class UPaperTerrainComponent> RenderComponent;
public:

	// AActor interface
#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
	// End of AActor interface

	/** Returns DummyRoot subobject **/
	FORCEINLINE class USceneComponent* GetDummyRoot() const { return DummyRoot; }
	/** Returns SplineComponent subobject **/
	FORCEINLINE class UPaperTerrainSplineComponent* GetSplineComponent() const { return SplineComponent; }
	/** Returns RenderComponent subobject **/
	FORCEINLINE class UPaperTerrainComponent* GetRenderComponent() const { return RenderComponent; }
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#endif
