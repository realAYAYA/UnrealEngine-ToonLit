// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/ShaderResourceManager.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingPolicy.h"

class FSlateRHIRenderingPolicy;

/**
 * Class used to expose only limited parts of the FSlateRenderingPolicy. 
 * Not an interface to be implemented.
 */
class SLATERHIRENDERER_API FSlateRHIRenderingPolicyInterface
{
public:
	FSlateRHIRenderingPolicyInterface(FSlateRHIRenderingPolicy* InRenderingPolicy);

	static int32 GetProcessSlatePostBuffers();

	bool IsValid() const;
	bool IsVertexColorInLinearSpace() const;
	bool GetApplyColorDeficiencyCorrection() const;

	void BlurRectExternal(FRHICommandListImmediate& RHICmdList, FRHITexture* BlurSrc, FRHITexture* BlurDst, FIntRect SrcRect, FIntRect DstRect, float BlurStrength) const;

private:

	/** Rendering policy we are an interface to */
	FSlateRHIRenderingPolicy* RenderingPolicy;
};

/**
 * RHI version of custom slate element that can accept additional RHI params when performing draw
 */
class ICustomSlateElementRHI : public ICustomSlateElement
{
public:

	/**
	 * Called from the rendering thread when it is time to render the element
	 *
	 * @param RenderTarget				handle to the platform specific render target implementation.  Note this is already bound by Slate initially
	 * @param Params					Params about current draw state
	 * @param RenderingPolicyInterface	Interface to current rendering policy
	 */
	virtual void Draw_RHIRenderThread(class FRHICommandListImmediate& RHICmdList, const FTextureRHIRef& RenderTarget, const FSlateCustomDrawParams& Params, FSlateRHIRenderingPolicyInterface RenderingPolicyInterface)
	{
	}

	//~ Begin ICustomSlateElement interface
	virtual bool UsesAdditionalRHIParams() const override
	{
		return true;
	}
	//~ End ICustomSlateElement interface
};