// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/SlateRHIRenderingPolicyInterface.h"
#include "SlateRHIRenderingPolicy.h"
#include "SlateRHIRenderer.h"

FSlateRHIRenderingPolicyInterface::FSlateRHIRenderingPolicyInterface(FSlateRHIRenderingPolicy* InRenderingPolicy)
	: RenderingPolicy(InRenderingPolicy)
{
}

int32 FSlateRHIRenderingPolicyInterface::GetProcessSlatePostBuffers()
{
	return FSlateRHIRenderer::GetProcessSlatePostBuffers();
}

bool FSlateRHIRenderingPolicyInterface::IsValid() const
{
	return RenderingPolicy != nullptr;
}

bool FSlateRHIRenderingPolicyInterface::IsVertexColorInLinearSpace() const
{
	if (RenderingPolicy)
	{
		return RenderingPolicy->IsVertexColorInLinearSpace();
	}

	return false;
}

bool FSlateRHIRenderingPolicyInterface::GetApplyColorDeficiencyCorrection() const
{
	if (RenderingPolicy)
	{
		return RenderingPolicy->GetApplyColorDeficiencyCorrection();
	}

	return false;
}

void FSlateRHIRenderingPolicyInterface::BlurRectExternal(FRHICommandListImmediate& RHICmdList, FRHITexture* BlurSrc, FRHITexture* BlurDst, FIntRect SrcRect, FIntRect DstRect, float BlurStrength) const
{
	if (RenderingPolicy)
	{
		RenderingPolicy->BlurRectExternal(RHICmdList, BlurSrc, BlurDst, SrcRect, DstRect, BlurStrength);
	}
}