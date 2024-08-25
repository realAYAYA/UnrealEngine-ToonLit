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
	bSkipNearDefaultValues = true;
	bIncludeCopyrightNotice = false;
	bExportProxyMaterials = true;
	bUseImporterMaterialMapping = true;
	bExportUnlitMaterials = true;
	bExportClearCoatMaterials = true;
	bExportClothMaterials = true;
	bExportThinTranslucentMaterials = false;
	bExportEmissiveStrength = true;
	bExportSpecularGlossinessMaterials = true;
	BakeMaterialInputs = EGLTFMaterialBakeMode::UseMeshData;
	DefaultMaterialBakeSize = FGLTFMaterialBakeSize::Default;
	DefaultMaterialBakeFilter = TF_Trilinear;
	DefaultMaterialBakeTiling = TA_Wrap;
	DefaultLevelOfDetail = 0;
	bExportVertexColors = false;
	bExportVertexSkinWeights = true;
	bMakeSkinnedMeshesRoot = true;
	bUseMeshQuantization = false;
	bExportLevelSequences = true;
	bExportAnimationSequences = true;
	TextureImageFormat = EGLTFTextureImageFormat::PNG;
	TextureImageQuality = 0;
	bExportTextureTransforms = true;
	bAdjustNormalmaps = true;
	bExportHiddenInGame = false;
	bExportLights = true;
	bExportCameras = true;
	ExportMaterialVariants = EGLTFMaterialVariantMode::UseMeshData;
}
