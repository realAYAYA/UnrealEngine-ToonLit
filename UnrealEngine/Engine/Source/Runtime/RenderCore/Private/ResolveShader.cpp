// Copyright Epic Games, Inc. All Rights Reserved.


#include "ResolveShader.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_SHADER_TYPE(, FResolveDepthPS, TEXT("/Engine/Private/ResolvePixelShader.usf"), TEXT("MainDepth"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FResolveDepth2XPS, TEXT("/Engine/Private/ResolvePixelShader.usf"), TEXT("MainDepth"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FResolveDepth4XPS, TEXT("/Engine/Private/ResolvePixelShader.usf"), TEXT("MainDepth"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FResolveDepth8XPS, TEXT("/Engine/Private/ResolvePixelShader.usf"), TEXT("MainDepth"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FResolveSingleSamplePS, TEXT("/Engine/Private/ResolvePixelShader.usf"), TEXT("MainSingleSample"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FResolveVS, TEXT("/Engine/Private/ResolveVertexShader.usf"), TEXT("Main"), SF_Vertex);

void FResolveSingleSamplePS::SetParameters(FRHICommandList& RHICmdList, uint32 SingleSampleIndexValue)
{
	SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(),SingleSampleIndex,SingleSampleIndexValue);
}

void FResolveVS::SetParameters(FRHICommandList& RHICmdList, const FResolveRect& SrcBounds, const FResolveRect& DstBounds, uint32 DstSurfaceWidth, uint32 DstSurfaceHeight)
{
	// Generate the vertices used to copy from the source surface to the destination surface.
	const float MinU = (float)SrcBounds.X1;
	const float MinV = (float)SrcBounds.Y1;
	const float MaxU = (float)SrcBounds.X2;
	const float MaxV = (float)SrcBounds.Y2;
	const float MinX = -1.f + DstBounds.X1 / ((float)DstSurfaceWidth * 0.5f);
	const float MinY = +1.f - DstBounds.Y1 / ((float)DstSurfaceHeight * 0.5f);
	const float MaxX = -1.f + DstBounds.X2 / ((float)DstSurfaceWidth * 0.5f);
	const float MaxY = +1.f - DstBounds.Y2 / ((float)DstSurfaceHeight * 0.5f);

	SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), PositionMinMax, FVector4f(MinX, MinY, MaxX, MaxY));
	SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), UVMinMax, FVector4f(MinU, MinV, MaxU, MaxV));
}
