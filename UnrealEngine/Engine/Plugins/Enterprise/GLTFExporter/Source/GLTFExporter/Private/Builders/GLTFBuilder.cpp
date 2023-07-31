// Copyright Epic Games, Inc. All Rights Reserved.

#include "Builders/GLTFBuilder.h"
#include "Converters/GLTFMeshUtility.h"
#include "UserData/GLTFMaterialUserData.h"

FGLTFBuilder::FGLTFBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions)
	: FileName(FPaths::GetCleanFilename(FileName))
	, bIsGLB(this->FileName.EndsWith(TEXT(".glb"), ESearchCase::IgnoreCase))
	, ExportOptions(SanitizeExportOptions(ExportOptions))
	, ExportOptionsGuard(this->ExportOptions)
{
}

const UMaterialInterface* FGLTFBuilder::ResolveProxy(const UMaterialInterface* Material) const
{
	return ExportOptions->bExportProxyMaterials ? UGLTFMaterialExportOptions::ResolveProxy(Material) : Material;
}

void FGLTFBuilder::ResolveProxies(TArray<const UMaterialInterface*>& Materials) const
{
	if (ExportOptions->bExportProxyMaterials)
	{
		for (const UMaterialInterface*& Material : Materials)
		{
			Material = UGLTFMaterialExportOptions::ResolveProxy(Material);
		}
	}
}

FIntPoint FGLTFBuilder::GetBakeSizeForMaterialProperty(const UMaterialInterface* Material, EGLTFMaterialPropertyGroup PropertyGroup) const
{
	EGLTFMaterialBakeSizePOT DefaultValue = ExportOptions->DefaultMaterialBakeSize;
	if (const FGLTFOverrideMaterialBakeSettings* BakeSettings = ExportOptions->DefaultInputBakeSettings.Find(PropertyGroup))
	{
		if (BakeSettings->bOverrideSize)
		{
			DefaultValue = BakeSettings->Size;
		}
	}

	// TODO: add option to ignore override
	const EGLTFMaterialBakeSizePOT Size = UGLTFMaterialExportOptions::GetBakeSizeForPropertyGroup(Material, PropertyGroup, DefaultValue);
	const int32 PixelSize = 1 << static_cast<uint8>(Size);
	return { PixelSize, PixelSize };
}

TextureFilter FGLTFBuilder::GetBakeFilterForMaterialProperty(const UMaterialInterface* Material, EGLTFMaterialPropertyGroup PropertyGroup) const
{
	TextureFilter DefaultValue = ExportOptions->DefaultMaterialBakeFilter;
	if (const FGLTFOverrideMaterialBakeSettings* BakeSettings = ExportOptions->DefaultInputBakeSettings.Find(PropertyGroup))
	{
		if (BakeSettings->bOverrideFilter)
		{
			DefaultValue = BakeSettings->Filter;
		}
	}

	// TODO: add option to ignore override
	return UGLTFMaterialExportOptions::GetBakeFilterForPropertyGroup(Material, PropertyGroup, DefaultValue);
}

TextureAddress FGLTFBuilder::GetBakeTilingForMaterialProperty(const UMaterialInterface* Material, EGLTFMaterialPropertyGroup PropertyGroup) const
{
	TextureAddress DefaultValue = ExportOptions->DefaultMaterialBakeTiling;
	if (const FGLTFOverrideMaterialBakeSettings* BakeSettings = ExportOptions->DefaultInputBakeSettings.Find(PropertyGroup))
	{
		if (BakeSettings->bOverrideTiling)
		{
			DefaultValue = BakeSettings->Tiling;
		}
	}

	// TODO: add option to ignore override
	return UGLTFMaterialExportOptions::GetBakeTilingForPropertyGroup(Material, PropertyGroup, DefaultValue);
}

EGLTFJsonHDREncoding FGLTFBuilder::GetTextureHDREncoding() const
{
	switch (ExportOptions->TextureHDREncoding)
	{
		case EGLTFTextureHDREncoding::None: return EGLTFJsonHDREncoding::None;
		case EGLTFTextureHDREncoding::RGBM: return EGLTFJsonHDREncoding::RGBM;
		// TODO: add more encodings (like RGBE) when viewer supports them
		default:
			checkNoEntry();
			return EGLTFJsonHDREncoding::None;
	}
}

bool FGLTFBuilder::ShouldExportLight(EComponentMobility::Type LightMobility) const
{
	const EGLTFSceneMobility AllowedMobility = static_cast<EGLTFSceneMobility>(ExportOptions->ExportLights);
	const EGLTFSceneMobility QueriedMobility = GetSceneMobility(LightMobility);
	return EnumHasAllFlags(AllowedMobility, QueriedMobility);
}

int32 FGLTFBuilder::SanitizeLOD(const UStaticMesh* StaticMesh, const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex) const
{
	return LODIndex > 0
		? FMath::Min(LODIndex, FGLTFMeshUtility::GetMaximumLOD(StaticMesh))
		: FGLTFMeshUtility::GetLOD(StaticMesh, StaticMeshComponent, ExportOptions->DefaultLevelOfDetail);
}

int32 FGLTFBuilder::SanitizeLOD(const USkeletalMesh* SkeletalMesh, const USkeletalMeshComponent* SkeletalMeshComponent, int32 LODIndex) const
{
	return LODIndex > 0
		? FMath::Min(LODIndex, FGLTFMeshUtility::GetMaximumLOD(SkeletalMesh))
		: FGLTFMeshUtility::GetLOD(SkeletalMesh, SkeletalMeshComponent, ExportOptions->DefaultLevelOfDetail);
}

const UGLTFExportOptions* FGLTFBuilder::SanitizeExportOptions(const UGLTFExportOptions* Options)
{
	if (Options == nullptr)
	{
		UGLTFExportOptions* NewOptions = NewObject<UGLTFExportOptions>();
		NewOptions->ResetToDefault();
		Options = NewOptions;
	}

	if (!FApp::CanEverRender())
	{
		if (Options->BakeMaterialInputs != EGLTFMaterialBakeMode::Disabled || Options->TextureImageFormat != EGLTFTextureImageFormat::None)
		{
			// TODO: warn the following options requires rendering support and will be overriden
			UGLTFExportOptions* OverridenOptions = DuplicateObject(Options, nullptr);
			OverridenOptions->BakeMaterialInputs = EGLTFMaterialBakeMode::Disabled;
			OverridenOptions->TextureImageFormat = EGLTFTextureImageFormat::None;
			Options = OverridenOptions;
		}
	}

	return Options;
}

EGLTFSceneMobility FGLTFBuilder::GetSceneMobility(EComponentMobility::Type Mobility)
{
	switch (Mobility)
	{
		case EComponentMobility::Static:     return EGLTFSceneMobility::Static;
		case EComponentMobility::Stationary: return EGLTFSceneMobility::Stationary;
		case EComponentMobility::Movable:    return EGLTFSceneMobility::Movable;
		default:
			checkNoEntry();
			return EGLTFSceneMobility::None;
	}
}
