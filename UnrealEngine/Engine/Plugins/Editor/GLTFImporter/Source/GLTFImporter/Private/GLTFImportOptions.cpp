// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFImportOptions.h"

#include "CoreTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GLTFImportOptions)

UGLTFImportOptions::UGLTFImportOptions(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , bGenerateLightmapUVs(false)
    , ImportScale(100.f)
{
}

