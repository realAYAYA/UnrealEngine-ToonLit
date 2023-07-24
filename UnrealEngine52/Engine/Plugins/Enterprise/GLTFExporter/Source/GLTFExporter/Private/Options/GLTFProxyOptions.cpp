// Copyright Epic Games, Inc. All Rights Reserved.

#include "Options/GLTFProxyOptions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GLTFProxyOptions)

UGLTFProxyOptions::UGLTFProxyOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ResetToDefault();
}

void UGLTFProxyOptions::ResetToDefault()
{
	bBakeMaterialInputs = true;
	DefaultMaterialBakeSize = EGLTFMaterialBakeSizePOT::POT_1024;
	DefaultMaterialBakeFilter = TF_Trilinear;
	DefaultMaterialBakeTiling = TA_Wrap;
}
