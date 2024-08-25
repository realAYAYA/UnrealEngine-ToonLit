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

	UPROPERTY(VisibleAnywhere, Category = LandscapeLayerInfoObject, AssetRegistrySearchable)
	FName LayerName;

	UPROPERTY(EditAnywhere, Category = LandscapeLayerInfoObject, Meta = (DisplayName = "Physical Material", Tooltip = "Physical material to use when this layer is the predominant one at a given location. Note: this is ignored if the Landscape Physical Material node is used in the landscape material. "))
	TObjectPtr<UPhysicalMaterial> PhysMaterial;

	UPROPERTY(EditAnywhere, Category = LandscapeLayerInfoObject, Meta = (ClampMin = "0", ClampMax = "1", Tooltip = "Defines how much 'resistance' areas painted with this layer will offer to the Erosion tool. A hardness of 0 means the layer is fully affected by erosion, while 1 means fully unaffected."))
	float Hardness;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = LandscapeLayerInfoObject, Meta = (ClampMin = "0", ClampMax = "1", Tooltip = "The minimum weight that needs to be painted for that layer to be picked up as the dominant physical layer."))
	float MinimumCollisionRelevanceWeight;

	UPROPERTY(EditAnywhere, Category = LandscapeLayerInfoObject, Meta = (Tooltip = "Prevents this layer to be weight-blended with others."))
	uint32 bNoWeightBlend:1;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName="Texture", Tooltip = "Texture to modulate the Splines Falloff Layer Alpha"))
	TObjectPtr<UTexture2D> SplineFalloffModulationTexture;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName = "Color Mask", Tooltip = "Defines which channel of the Spline Falloff Modulation Texture to use."))
	ESplineModulationColorMask SplineFalloffModulationColorMask;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName = "Tiling", ClampMin = "0.01", Tooltip = "Defines the tiling to use when sampling the Spline Falloff Modulation Texture."))
	float SplineFalloffModulationTiling;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName = "Bias", ClampMin = "0", Tooltip = "Defines the offset to use when sampling the Spline Falloff Modulation Texture."))
	float SplineFalloffModulationBias;

	UPROPERTY(EditAnywhere, Category = SplineFalloffModulation, Meta = (DisplayName = "Scale", ClampMin = "0", Tooltip = "Allows to scale the value sampled from the Spline Falloff Modulation Texture."))
	float SplineFalloffModulationScale;
				
	UPROPERTY(NonTransactional, Transient)
	bool IsReferencedFromLoadedData;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category = LandscapeLayerInfoObject, Meta = (Tooltip = "The color to use for layer usage debug"))
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
