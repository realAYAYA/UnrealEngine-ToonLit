// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "LandscapeLayerInfoObject.generated.h"

class UPhysicalMaterial;
class UTexture2D;
struct FPropertyChangedEvent;

UENUM()
enum class ESplineModulationColorMask : uint8
{
	Red,
	Green,
	Blue,
	Alpha
};

UCLASS(MinimalAPI, BlueprintType)
class ULandscapeLayerInfoObject : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, Category=LandscapeLayerInfoObject, AssetRegistrySearchable)
	FName LayerName;

	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	TObjectPtr<UPhysicalMaterial> PhysMaterial;

	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	float Hardness;

#if WITH_EDITORONLY_DATA
	/* The minimum weight that needs to be painted for that layer to be picked up as the dominant physical layer */
	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject, Meta = (ClampMin="0", ClampMax="1"))
	float MinimumCollisionRelevanceWeight;

	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	uint32 bNoWeightBlend:1;

	/** Texture to modulate the Splines Falloff Layer Alpha */
	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName="Texture"))
	TObjectPtr<UTexture2D> SplineFalloffModulationTexture;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName = "Color Mask"))
	ESplineModulationColorMask SplineFalloffModulationColorMask;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName = "Tiling", ClampMin="0.01"))
	float SplineFalloffModulationTiling;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName = "Bias", ClampMin="0"))
	float SplineFalloffModulationBias;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName = "Scale", ClampMin="0"))
	float SplineFalloffModulationScale;
				
	UPROPERTY(NonTransactional, Transient)
	bool IsReferencedFromLoadedData;
#endif // WITH_EDITORONLY_DATA

	/* The color to use for layer usage debug */
	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	FLinearColor LayerUsageDebugColor;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostLoad() override;
	//~ End UObject Interface
#endif

	LANDSCAPE_API FLinearColor GenerateLayerUsageDebugColor() const;
};
