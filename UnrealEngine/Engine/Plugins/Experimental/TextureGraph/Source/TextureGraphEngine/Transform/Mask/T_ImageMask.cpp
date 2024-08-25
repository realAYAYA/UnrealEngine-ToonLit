// Copyright Epic Games, Inc. All Rights Reserved.
#include "T_ImageMask.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "Transform/Utility/T_MinMax.h"

IMPLEMENT_GLOBAL_SHADER(FSH_ImageMask, "/Plugin/TextureGraph/Mask/ImageMask.usf", "FSH_ImageMask", SF_Pixel);

template <>
void SetupDefaultParameters(FSH_ImageMask::FParameters& params) {
	FStandardSamplerStates_Setup(params.SamplerStates);
}

//////////////////////////////////////////////////////////////////////////
T_ImageMask::T_ImageMask()
{
}

T_ImageMask::~T_ImageMask()
{
}

