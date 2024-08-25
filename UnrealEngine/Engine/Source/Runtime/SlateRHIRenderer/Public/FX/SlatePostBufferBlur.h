// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FX/SlateRHIPostBufferProcessor.h"

#include "SlatePostBufferBlur.generated.h"


/**
 * Proxy for post buffer processor that the renderthread uses to perform processing
 * This proxy exists because generally speaking usage on UObjects on the renderthread
 * is a race condition due to UObjects being managed / updated by the game thread
 */
class SLATERHIRENDERER_API FSlatePostBufferBlurProxy : public FSlateRHIPostBufferProcessorProxy
{

public:

	//~ Begin FSlateRHIPostBufferProcessorProxy Interface
	virtual void PostProcess_Renderthread(FRHICommandListImmediate& RHICmdList, FRHITexture* Src, FRHITexture* Dst, FIntRect SrcRect, FIntRect DstRect, FSlateRHIRenderingPolicyInterface InRenderingPolicy) override;
	virtual void OnUpdateValuesRenderThread() override;
	//~ End FSlateRHIPostBufferProcessorProxy Interface

protected:

	/** Blur strength to use when processing, renderthread version actually used to draw. Must be updated via render command except during initialization. */
	float GaussianBlurStrength_RenderThread = 10;

	/** Fence to allow for us to queue only one update per draw command */
	FRenderCommandFence ParamUpdateFence;
};

/**
 * Slate Post Buffer Processor that performs a simple gaussian blur to the backbuffer
 * 
 * Create a new asset deriving from this class to use / modify settings.
 */
UCLASS(Abstract, Blueprintable, CollapseCategories)
class SLATERHIRENDERER_API USlatePostBufferBlur : public USlateRHIPostBufferProcessor
{
	GENERATED_BODY()

public:

	UPROPERTY(interp, BlueprintReadWrite, Category = "GaussianBlur")
	float GaussianBlurStrength = 10;

public:

	USlatePostBufferBlur();
	virtual ~USlatePostBufferBlur() override;

	//~ Begin USlateRHIPostBufferProcessor Interface
	virtual void PostProcess(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FSlateRHIRenderingPolicyInterface InRenderingPolicy, UTextureRenderTarget2D* InSlatePostBuffer) override;
	virtual TSharedPtr<FSlateRHIPostBufferProcessorProxy> GetRenderThreadProxy();
	//~ End USlateRHIPostBufferProcessor Interface

private:

	TSharedPtr<FSlateRHIPostBufferProcessorProxy> RenderThreadProxy;
};