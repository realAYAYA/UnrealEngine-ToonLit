// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"

struct FRGBToYUVShaderParameters
{
	FTextureRHIRef SourceTexture;

	FIntPoint DestPlaneYDimensions;
	FIntPoint DestPlaneUVDimensions;

	FUnorderedAccessViewRHIRef DestPlaneY;
	FUnorderedAccessViewRHIRef DestPlaneU;
	FUnorderedAccessViewRHIRef DestPlaneV;
};

class PIXELCAPTURESHADERS_API FRGBToYUVShader
{
public:
	static void Dispatch(FRHICommandListImmediate& RHICmdList, const FRGBToYUVShaderParameters& InParameters);
};
