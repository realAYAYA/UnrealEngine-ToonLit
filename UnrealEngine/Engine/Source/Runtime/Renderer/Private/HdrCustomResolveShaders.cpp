// Copyright Epic Games, Inc. All Rights Reserved.

#include "HdrCustomResolveShaders.h"

IMPLEMENT_SHADER_TYPE(,FHdrCustomResolveVS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolveVS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve2xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolve2xPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve4xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolve4xPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve8xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolve8xPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolveFMask2xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolveFMaskPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolveFMask4xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolveFMaskPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolveFMask8xPS,TEXT("/Engine/Private/HdrCustomResolveShaders.usf"),TEXT("HdrCustomResolveFMaskPS"),SF_Pixel);

