// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHIDefinitions.h"
#include "RenderGraph.h"
#include "HairCardsDatas.h"
#include "HairStrandsDatas.h"
#include "GroomAssetCards.h"
#include "GroomResources.h"

struct FHairGroupCardsTextures;
struct FHairStrandsVoxelData;

#if WITH_EDITOR
namespace FHairCardsBuilder
{
	bool ImportGeometry(
		const UStaticMesh* StaticMesh,
		const FHairStrandsDatas& InStrandsData,
		const FHairStrandsVoxelData& InStrandsVoxelData,
		FHairCardsBulkData& OutBulk,
		FHairStrandsDatas& OutGuides,
		FHairCardsInterpolationBulkData& OutInterpolationBulkData);

	HAIRSTRANDSCORE_API bool ExtractCardsData(
		const UStaticMesh* StaticMesh, 
		const FHairStrandsDatas& InStrandsData,
		FHairCardsDatas& Out);

	void ExportGeometry(
		const FHairCardsDatas& InCardsData, 
		UStaticMesh* OutStaticMesh);

	void BuildGeometry(
		const FString& LODName,
		const struct FHairStrandsDatas& InRen,
		const struct FHairStrandsDatas& InSim,
		const struct FHairGroupsProceduralCards& Settings,
		FHairCardsProceduralDatas& Out,
		FHairCardsBulkData& OutBulk,
		FHairStrandsDatas& OutGuides,
		FHairCardsInterpolationBulkData& OutInterpolationBulk,
		FHairGroupCardsTextures& OutTextures);

	void BuildTextureAtlas(
		FHairCardsProceduralDatas* ProceduralData,
		FHairCardsRestResource* RestResource,
		FHairCardsProceduralResource* AtlasResource,
		FHairGroupCardsTextures* Textures);

	FString GetVersion();
}

namespace FHairMeshesBuilder
{
	void BuildGeometry(
		const FBox& InBox,
		FHairMeshesBulkData& OutBulk);

	void ImportGeometry(
		const UStaticMesh* StaticMesh,
		FHairMeshesBulkData& OutBulk);

	FString GetVersion();
}
#endif // WITH_EDITOR