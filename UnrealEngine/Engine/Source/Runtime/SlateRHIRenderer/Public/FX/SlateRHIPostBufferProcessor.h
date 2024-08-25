// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "RHIFwd.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Interfaces/SlateRHIRenderingPolicyInterface.h"

#include "SlateRHIPostBufferProcessor.generated.h"

class USlateRHIPostBufferProcessor;

/**
 * Proxy for post buffer processor that the renderthread uses to perform processing
 * This proxy exists because generally speaking usage on UObjects on the renderthread
 * is a race condition due to UObjects being managed / updated by the game thread
 */
class SLATERHIRENDERER_API FSlateRHIPostBufferProcessorProxy : public TSharedFromThis<FSlateRHIPostBufferProcessorProxy>
{

public:

	virtual ~FSlateRHIPostBufferProcessorProxy()
	{
	}

	/**
	 * Called directly inside renderthread to perform some processing, do not enque commands here as we are already in the renderthread
	 *
	 * @param Src					Source texture to process as Input
	 * @param Dst					Destination texture to store process output
	 * @param SrcRect				Rect within source texture to sample, in PIE this is a subsection, in standlone it should be the entire texture
	 * @param DstRect				Rect within output to write out, since this is within the renderthread should almost always be Dst's Extent.
	 * @param InRenderingPolicy		RenderingPolicy used to assist / perform processing
	 */
	virtual void PostProcess_Renderthread(FRHICommandListImmediate& RHICmdList, FRHITexture* Src, FRHITexture* Dst, FIntRect SrcRect, FIntRect DstRect, FSlateRHIRenderingPolicyInterface InRenderingPolicy)
	{
	}

	/** 
	 * Called when an post buffer update element is added to a renderbatch, 
	 * gives proxies a chance to queue updates to their renderthread values based on the UObject processor.
	 * These updates should likely be guarded by an 'FRenderCommandFence' to avoid duplicate updates
	 */
	virtual void OnUpdateValuesRenderThread()
	{
	}

	/**
	 * Set the UObject that we are a renderthread proxy for, useful for doing gamethread updates from the proxy
	 */
	void SetOwningProcessorObject(USlateRHIPostBufferProcessor* InParentObject)
	{
		ParentObject = InParentObject;
	}

protected:

	/** Pointer to processor that we are a proxy for, external design constraints should ensure that this is always valid */
	TWeakObjectPtr<USlateRHIPostBufferProcessor> ParentObject;
};

/**
 * Base class for types that can process the backbuffer scene into the slate post buffer.
 * 
 * Implement 'PostProcess' in your derived class. Additionally, you need to create a renderthread proxy that derives from 'FSlateRHIPostBufferProcessorProxy'
 * For an example see: USlatePostBufferBlur.
 */
UCLASS(Abstract, Blueprintable, CollapseCategories)
class SLATERHIRENDERER_API USlateRHIPostBufferProcessor : public UObject
{
	GENERATED_BODY()

public:

	virtual ~USlateRHIPostBufferProcessor()
	{
	}

	/**
	 * Overridable postprocess for the given source scene backbuffer provided in 'Src' into 'Dst'
	 * You must override this method. In your override, you should copy params before executing 'ENQUEUE_RENDER_COMMAND'.
	 * This allows you to avoid render & game thread race conditions. See 'SlatePostBufferBlur' for example.
	 * Also avoid capturing [this] in your override, to avoid possible GC issues with the processor instance.
	 *
	 * @param InViewInfo			'FViewportInfo' resource used to get backbuffer in standalone
	 * @param InViewportTexture		'FSlateRenderTargetRHI' resource used to get the 'BufferedRT' viewport texture used in PIE
	 * @param InElementWindowSize	Size of window being rendered, used to determine if using stereo rendering or not.
	 * @param InRenderingPolicy		Slate RHI RenderingPolicy
	 * @param InSlatePostBuffer		Texture render target used for final output
	 */
	virtual void PostProcess(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FSlateRHIRenderingPolicyInterface InRenderingPolicy, UTextureRenderTarget2D* InSlatePostBuffer)
	{
	}

	/**
	 * Gets proxy for this post buffer processor, for execution on the renderthread
	 */
	virtual TSharedPtr<FSlateRHIPostBufferProcessorProxy> GetRenderThreadProxy()
	{
		return nullptr;
	}

protected:

	/** 
	 * Gets scene backbuffer, typically used as 'Src' texture for post process, but not always (Ex: PIE).
	 * 
	 * @param InViewInfo			'FViewportInfo' resource used to get backbuffer in standalone
	 * @param InViewportTexture		'FSlateRenderTargetRHI' resource used to get the 'BufferedRT' viewport texture used in PIE
	 * @param InElementWindowSize	Size of window being rendered, used to determine if using stereo rendering or not.
	 * @param InRHICmdList			RHI command list to queue commands on
	 */
	static FTexture2DRHIRef GetBackbuffer_RenderThread(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FRHICommandListImmediate& InRHICmdList);

	/**
	 * Gets 'Src' texture for post process command. Typically the scenebuffer.
	 *
	 * @param InBackBuffer			Backbuffer used in standalone
	 * @param InViewportTexture		'FSlateRenderTargetRHI' resource used for 'BufferedRT' viewport texture in PIE
	 */
	static FTexture2DRHIRef GetSrcTexture_RenderThread(FTexture2DRHIRef InBackBuffer, FRenderResource* InViewportTexture);

	/**
	 * Gets 'Dst' texture for post process command. Convience method, this should be possible through the direct resource.
	 *
	 * @param InSlatePostBuffer		Texture render target used for final output
	 */
	static FTextureReferenceRHIRef& GetDstTexture_RenderThread(UTextureRenderTarget2D* InSlatePostBuffer);

	/**
	 * Gets 'Dst' extent. Used for final size in post process command.
	 *
	 * @param InBackBuffer			Backbuffer used for size in standalone
	 * @param InViewportTexture		'FSlateRenderTargetRHI' resource used for 'BufferedRT' viewport texture size in PIE
	 */
	static FIntPoint GetDstExtent_RenderThread(FTexture2DRHIRef InBackBuffer, FRenderResource* InViewportTexture);
};