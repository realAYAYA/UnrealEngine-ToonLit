// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TranslucentPassResource.h"
#include "PathTracingResources.h"

struct FPostProcessingInputs
{
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures = nullptr;
	FRDGTextureRef ViewFamilyTexture = nullptr;
	FRDGTextureRef CustomDepthTexture = nullptr;
	FRDGTextureRef ExposureIlluminance = nullptr;
	FTranslucencyViewResourcesMap TranslucencyViewResourcesMap;
	FPathTracingResources PathTracingResources;

	bool bSeparateCustomStencil = false;

	void Validate() const
	{
		check(SceneTextures);
		check(ViewFamilyTexture);
		check(TranslucencyViewResourcesMap.IsValid());
	}
};
