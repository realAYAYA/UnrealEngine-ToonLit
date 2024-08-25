// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneView.h"
#include "RenderGraphUtils.h"
#include "CustomDepthRendering.h"
#include "SceneRenderTargetParameters.h"
#include "GBufferInfo.h"

struct FSceneTextures;
class FViewInfo;
class FViewFamilyInfo;

/** Initializes a scene textures config instance from the view family. */
extern RENDERER_API void InitializeSceneTexturesConfig(FSceneTexturesConfig& Config, const FSceneViewFamily& ViewFamily);

/** RDG struct containing the minimal set of scene textures common across all rendering configurations. */
struct FMinimalSceneTextures
{
	// Initializes the minimal scene textures structure in the FViewFamilyInfo
	static RENDERER_API void InitializeViewFamily(FRDGBuilder& GraphBuilder, FViewFamilyInfo& ViewFamily);

	// Immutable copy of the config used to create scene textures.
	FSceneTexturesConfig Config;

	// Uniform buffers for deferred or mobile.
	TRDGUniformBufferRef<FSceneTextureUniformParameters> UniformBuffer{};
	TRDGUniformBufferRef<FMobileSceneTextureUniformParameters> MobileUniformBuffer{};

	// Setup modes used when creating uniform buffers. These are updated on demand.
	ESceneTextureSetupMode SetupMode = ESceneTextureSetupMode::None;
	EMobileSceneTextureSetupMode MobileSetupMode = EMobileSceneTextureSetupMode::None;

	// Texture containing scene color information with lighting but without post processing. Will be two textures if MSAA.
	FRDGTextureMSAA Color{};

	// Texture containing scene depth. Will be two textures if MSAA.
	FRDGTextureMSAA Depth{};

	// Texture containing a stencil view of the resolved (if MSAA) scene depth. 
	FRDGTextureSRVRef Stencil{};

	// Textures containing primary depth buffer copied before other meshes are rendered in the secondary depth pass.
	FRDGTextureMSAA PartialDepth{};

	// Textures containing depth / stencil information from the custom depth pass.
	FCustomDepthTextures CustomDepth{};

	RENDERER_API FSceneTextureShaderParameters GetSceneTextureShaderParameters(ERHIFeatureLevel::Type FeatureLevel) const;
};

/** RDG struct containing the complete set of scene textures for the deferred or mobile renderers. */
struct FSceneTextures : public FMinimalSceneTextures
{
	// Initializes the scene textures structure in the FViewFamilyInfo
	static RENDERER_API void InitializeViewFamily(FRDGBuilder& GraphBuilder, FViewFamilyInfo& ViewFamily);
	static RENDERER_API EPixelFormat GetGBufferFFormatAndCreateFlags(ETextureCreateFlags& OutCreateFlags);

	// Configures an array of render targets for the GBuffer pass.
	RENDERER_API uint32 GetGBufferRenderTargets(
		TArrayView<FTextureRenderTargetBinding> RenderTargets,
		EGBufferLayout Layout = GBL_Default) const;
	RENDERER_API uint32 GetGBufferRenderTargets(
		ERenderTargetLoadAction LoadAction,
		FRenderTargetBindingSlots& RenderTargets,
		EGBufferLayout Layout = GBL_Default) const;
	
	// (Deferred) Texture containing conservative downsampled depth for occlusion.
	FRDGTextureRef SmallDepth{};

	// (Deferred) Textures containing geometry information for deferred shading.
	FRDGTextureRef GBufferA{};
	FRDGTextureRef GBufferB{};
	FRDGTextureRef GBufferC{};
	FRDGTextureRef GBufferD{};
	FRDGTextureRef GBufferE{};
	FRDGTextureRef GBufferF{};

	// Additional Buffer texture used by mobile
	FRDGTextureMSAA DepthAux{};

	// Texture containing dynamic motion vectors. Can be bound by the base pass or its own velocity pass.
	FRDGTextureRef Velocity{};
	
	// (Mobile Local Light Prepass) Textures containing LocalLight Direction and Color
	FRDGTextureRef MobileLocalLightTextureA {};
	FRDGTextureRef MobileLocalLightTextureB {};

	// Texture containing the screen space ambient occlusion result.
	FRDGTextureRef ScreenSpaceAO{};

	// Texture used by the quad overdraw debug view mode when enabled.
	FRDGTextureRef QuadOverdraw{};

	// (Mobile) Texture used by mobile PPR in the next frame.
	FRDGTextureRef PixelProjectedReflection{};

	// Textures used to composite editor primitives. Also used by the base pass when in wireframe mode.
#if WITH_EDITOR
	FRDGTextureRef EditorPrimitiveColor{};
	FRDGTextureRef EditorPrimitiveDepth{};
#endif
};

/** Extracts scene textures into the global extraction instance. */
void QueueSceneTextureExtractions(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures);
