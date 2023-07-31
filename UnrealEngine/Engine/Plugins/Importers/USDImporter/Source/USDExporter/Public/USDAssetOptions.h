// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MaterialOptions.h"

#include "RHIDefinitions.h"

#include "USDAssetOptions.generated.h"

struct FAnalyticsEventAttribute;

USTRUCT( BlueprintType )
struct USDEXPORTER_API FUsdMaterialBakingOptions
{
	GENERATED_BODY()

	/** Properties which are supposed to be baked out for the material */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Material baking options" )
	TArray<FPropertyEntry> Properties = { MP_BaseColor, MP_Metallic, MP_Specular, MP_Roughness, MP_Anisotropy, MP_EmissiveColor, MP_Opacity, MP_OpacityMask, MP_Normal, MP_Tangent, MP_SubsurfaceColor, MP_AmbientOcclusion };

	/** Size of the baked texture for all properties that don't have a CustomSize set */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Material baking options", meta = ( ClampMin = "1", UIMin = "1" ) )
	FIntPoint DefaultTextureSize = FIntPoint{ 128, 128 };

	/** Where baked textures are placed. Intentionally not a config as it's heavily dependent on where the stage is */
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Material baking options" )
	FDirectoryPath TexturesDir;
};

USTRUCT( BlueprintType )
struct USDEXPORTER_API FUsdMeshAssetOptions
{
	GENERATED_BODY()

	/** If true, the mesh data is exported to yet another "payload" file, and referenced via a payload composition arc */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Mesh options" )
	bool bUsePayload = false;

	/** USD format to use for exported payload files */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Mesh options", meta = ( EditCondition = bUsePayload, GetOptions = "USDExporter.LevelExporterUSDOptions.GetUsdExtensions" ) )
	FString PayloadFormat;

	/** Whether to bake the mesh's assigned material and export these as separate UsdPreviewSurface assets */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Material options" )
	bool bBakeMaterials = false;

	/** Whether to remove the 'unrealMaterial' attribute after binding the corresponding baked material */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Material options", meta = ( EditCondition = bBakeMaterials ) )
	bool bRemoveUnrealMaterials = false;

	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Material options", meta = ( EditCondition = bBakeMaterials ) )
	FUsdMaterialBakingOptions MaterialBakingOptions;

	/** Lowest of the LOD indices to export static and skeletal meshes with (use 0 for full detail) */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Mesh options", meta = ( ClampMin = "0" ) )
	int32 LowestMeshLOD = 0;

	/** Highest of the LOD indices to export static and skeletal meshes with */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "Mesh options", meta = ( ClampMin = "0" ) )
	int32 HighestMeshLOD = MAX_MESH_LOD_COUNT - 1;
};

namespace UsdUtils
{
	USDEXPORTER_API void AddAnalyticsAttributes(
		const FUsdMaterialBakingOptions& Options,
		TArray< FAnalyticsEventAttribute >& InOutAttributes
	);

	USDEXPORTER_API void AddAnalyticsAttributes(
		const FUsdMeshAssetOptions& Options,
		TArray< FAnalyticsEventAttribute >& InOutAttributes
	);

	USDEXPORTER_API void HashForMaterialExport(
		const FUsdMaterialBakingOptions& Options,
		FSHA1& HashToUpdate
	);

	USDEXPORTER_API void HashForMeshExport(
		const FUsdMeshAssetOptions& Options,
		FSHA1& HashToUpdate
	);
}