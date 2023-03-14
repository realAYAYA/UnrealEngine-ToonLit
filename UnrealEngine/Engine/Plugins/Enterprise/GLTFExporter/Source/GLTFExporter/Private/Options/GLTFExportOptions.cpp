// Copyright Epic Games, Inc. All Rights Reserved.

#include "Options/GLTFExportOptions.h"

UGLTFExportOptions::UGLTFExportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ResetToDefault();
}

void UGLTFExportOptions::ResetToDefault()
{
	ExportUniformScale = 0.01;
	bExportPreviewMesh = true;
	bStrictCompliance = true;
	bSkipNearDefaultValues = true;
	bIncludeGeneratorVersion = true;
	bExportProxyMaterials = true;
	bExportUnlitMaterials = true;
	bExportClearCoatMaterials = true;
	bExportExtraBlendModes = false;
	BakeMaterialInputs = EGLTFMaterialBakeMode::UseMeshData;
	DefaultMaterialBakeSize = EGLTFMaterialBakeSizePOT::POT_1024;
	DefaultMaterialBakeFilter = TF_Trilinear;
	DefaultMaterialBakeTiling = TA_Wrap;
	DefaultLevelOfDetail = 0;
	bExportVertexColors = false;
	bExportVertexSkinWeights = true;
	bUseMeshQuantization = false;
	bExportLevelSequences = true;
	bExportAnimationSequences = true;
	bExportPlaybackSettings = false;
	TextureImageFormat = EGLTFTextureImageFormat::PNG;
	TextureImageQuality = 0;
	NoLossyImageFormatFor = static_cast<int32>(EGLTFTextureType::All);
	bExportTextureTransforms = true;
	bExportLightmaps = false;
	TextureHDREncoding = EGLTFTextureHDREncoding::None;
	bAdjustNormalmaps = true;
	bExportHiddenInGame = false;
	ExportLights = static_cast<int32>(EGLTFSceneMobility::Stationary | EGLTFSceneMobility::Movable);
	bExportCameras = true;
	bExportHDRIBackdrops = false;
	bExportSkySpheres = false;
	VariantSetsMode = EGLTFVariantSetsMode::None;
	ExportMaterialVariants = EGLTFMaterialVariantMode::UseMeshData;
	bExportMeshVariants = true;
	bExportVisibilityVariants = true;
}
