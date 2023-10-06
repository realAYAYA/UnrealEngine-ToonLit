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
	bExportUnlitMaterials = true;
	bExportClearCoatMaterials = true;
	bExportEmissiveStrength = true;
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
	TextureImageFormat = EGLTFTextureImageFormat::PNG;
	TextureImageQuality = 0;
	bExportTextureTransforms = true;
	bAdjustNormalmaps = true;
	bExportHiddenInGame = false;
	bExportLights = true;
	bExportCameras = true;
	ExportMaterialVariants = EGLTFMaterialVariantMode::UseMeshData;
}
