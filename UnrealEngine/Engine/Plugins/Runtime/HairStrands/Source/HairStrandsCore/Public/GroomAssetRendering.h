// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "GroomAssetRendering.generated.h"


class UMaterialInterface;


USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairGeometrySettings
{
	GENERATED_BODY()

	FHairGeometrySettings();

	/** Hair width (in centimeters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeometrySettings", meta = (ClampMin = "0.0001", UIMin = "0.001", UIMax = "1.0", SliderExponent = 6))
	float HairWidth;

	/** Scale the hair width at the root */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeometrySettings", AdvancedDisplay, meta = (ClampMin = "0.0001", UIMin = "0.001", UIMax = "2.0", SliderExponent = 6))
	float HairRootScale;

	/** Scale the hair with at the tip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeometrySettings", AdvancedDisplay, meta = (ClampMin = "0.0001", UIMin = "0.001", UIMax = "2.0", SliderExponent = 6))
	float HairTipScale;

	bool operator==(const FHairGeometrySettings& A) const;
};

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairShadowSettings
{
	GENERATED_BODY()

	FHairShadowSettings();

	/** Override the hair shadow density factor (unit less). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShadowSettings", AdvancedDisplay, meta = (ClampMin = "0.0001", UIMin = "0.001", UIMax = "10.0", SliderExponent = 6))
	float HairShadowDensity;

	/** Scale the hair geometry radius for ray tracing effects (e.g. shadow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShadowSettings", AdvancedDisplay, meta = (ClampMin = "0.0001", UIMin = "0.001", UIMax = "10.0", SliderExponent = 6))
	float HairRaytracingRadiusScale;

	/** Enable hair strands geomtry for raytracing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShadowSettings", AdvancedDisplay)
	bool bUseHairRaytracingGeometry;

	/** Enable stands voxelize for casting shadow and environment occlusion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShadowSettings", AdvancedDisplay)
	bool bVoxelize;

	bool operator==(const FHairShadowSettings& A) const;
};

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairAdvancedRenderingSettings
{
	GENERATED_BODY()

	FHairAdvancedRenderingSettings();

	/** Insure the hair does not alias. When enable, group of hairs might appear thicker. Isolated hair should remain thin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AdvancedRenderingSettings")
	bool bUseStableRasterization;

	/** Light hair with the scene color. This is used for vellus/short hair to bring light from the surrounding surface, like skin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AdvancedRenderingSettings")
	bool bScatterSceneLighting;

	bool operator==(const FHairAdvancedRenderingSettings& A) const;
};

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairGroupsRendering
{
	GENERATED_BODY()

	FHairGroupsRendering();

	UPROPERTY()
	FName MaterialSlotName;

	/* Deprecated */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, Category = "GeometrySettings", meta = (ToolTip = "Geometry settings"))
	FHairGeometrySettings GeometrySettings;

	UPROPERTY(EditAnywhere, Category = "ShadowSettings", meta = (ToolTip = "Shadow settings"))
	FHairShadowSettings ShadowSettings;

	UPROPERTY(EditAnywhere, Category = "MiscSettings", meta = (ToolTip = "Advanced rendering settings "))
	FHairAdvancedRenderingSettings AdvancedSettings;

	bool operator==(const FHairGroupsRendering& A) const;

};