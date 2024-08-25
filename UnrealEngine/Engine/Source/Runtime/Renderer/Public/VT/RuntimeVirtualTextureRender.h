// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VT/RuntimeVirtualTextureEnum.h"

class FRHICommandListImmediate;
class FRHITexture;
class FRHIUnorderedAccessView;
class FRDGBuilder;
class FRDGTexture;
class FRDGTextureUAV;
class FScene;
class FSceneInterface;
struct IPooledRenderTarget;
class URuntimeVirtualTextureComponent;

namespace RuntimeVirtualTexture
{
#if WITH_EDITOR
	/**
	 * Get the scene index of the FRuntimeVirtualTextureSceneProxy associated with a URuntimeVirtualTextureComponent.
	 * This is needed when rendering runtime virtual texture pages in alternative contexts such as when building previews etc.
	 * This function is slow because it needs to flush render commands.
	 */
	RENDERER_API uint32 GetRuntimeVirtualTextureSceneIndex_GameThread(URuntimeVirtualTextureComponent* InComponent);
#endif

	/** Enum for our maximum RenderPages() batch size. */
	enum { EMaxRenderPageBatch = 8 };

	/** Structure containing a texture layer target description for a call for RenderPages(). */
	struct FRenderPageTarget
	{
		/** Physical texture to render to. */
		FRHITexture* Texture = nullptr;

		UE_DEPRECATED(5.1, "UAV is deprecated. Register the pooled render target with RDG instead.")
		FRHIUnorderedAccessView* UAV = nullptr;

		IPooledRenderTarget* PooledRenderTarget = nullptr;
	};

	/** A single page description. Multiple of these can be placed in a single FRenderPageBatchDesc batch description. */
	struct FRenderPageDesc
	{
		/** vLevel to render at. */
		uint8 vLevel;
		/** UV range to render in virtual texture space. */
		FBox2D UVRange;
		/** Destination box to render in texel space of the target physical texture. */
		FBox2D DestBox[RuntimeVirtualTexture::MaxTextureLayers];
	};

	/** A description of a batch of pages to be rendered with a single call to RenderPages(). */
	struct FRenderPageBatchDesc
	{
		/** Scene to use when rendering the batch. */
		FScene* Scene;
		/** Mask created from the target runtime virtual texture's index within the scene. */
		uint32 RuntimeVirtualTextureMask;
		/** Virtual texture UV space to world space transform. */
		FTransform UVToWorld;
		/** Virtual texture world space bounds. */
		FBox WorldBounds;
		/** Material type of the runtime virtual texture that we are rendering. */
		ERuntimeVirtualTextureMaterialType MaterialType;
		/** Max mip level of the runtime virtual texture that we are rendering. */
		uint8 MaxLevel;
		/** Set to true to clear before rendering. */
		bool bClearTextures;
		/** Set to true for thumbnail rendering. */
		bool bIsThumbnails;
		/** Fixed BaseColor to apply. Uses alpha channel to blend with material output. */
		FLinearColor FixedColor;

		/** Physical texture targets to render to. */
		FRenderPageTarget Targets[RuntimeVirtualTexture::MaxTextureLayers];
		
		/** Number of pages to render. */
		int32 NumPageDescs;
		/** Page descriptions for each page in the batch. */
		FRenderPageDesc PageDescs[EMaxRenderPageBatch];
	};

	/** Returns true if the scene is initialized for rendering to runtime virtual textures. Always check this before calling RenderPages(). */
	RENDERER_API bool IsSceneReadyToRender(FSceneInterface* Scene);

	/** Render a batch of pages for a runtime virtual texture. */
	RENDERER_API void RenderPages(FRDGBuilder& GraphBuilder, FRenderPageBatchDesc const& InDesc);

	/** 
	 * Render a batch of pages for a runtime virtual texture. 
	 * Performs the required scene rendering setup such that it can be called from a render thread task.
	 */
	RENDERER_API void RenderPagesStandAlone(FRDGBuilder& GraphBuilder, FRenderPageBatchDesc const& InDesc);
}
