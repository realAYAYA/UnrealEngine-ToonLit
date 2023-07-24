// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraph.h"

enum class ECustomDepthPassLocation : uint32
{
	// Renders custom depth before the base pass. Can be more efficient with AsyncCompute and enables use with DBuffer decals.
	BeforeBasePass,

	// Renders after the base pass.
	AfterBasePass
};

// Returns the location in the frame where custom depth is rendered.
extern ECustomDepthPassLocation GetCustomDepthPassLocation(EShaderPlatform Platform);

enum class ECustomDepthMode : uint32
{
	// Custom depth is disabled.
	Disabled,

	// Custom depth is enabled.
	Enabled,

	// Custom depth is enabled and uses stencil.
	EnabledWithStencil,
};

// The custom depth mode currently configured.
extern ECustomDepthMode GetCustomDepthMode();

inline bool IsCustomDepthPassEnabled()
{
	return GetCustomDepthMode() != ECustomDepthMode::Disabled;
}

struct FCustomDepthTextures
{
	static FCustomDepthTextures Create(FRDGBuilder& GraphBuilder, FIntPoint CustomDepthExtent);

	bool IsValid() const
	{
		return Depth != nullptr;
	}

	FRDGTextureRef Depth{};
	FRDGTextureSRVRef Stencil{};

	// Denotes that the depth and stencil buffers had to be split to separate, non-depth textures (and thus Depth cannot be bound
	// as a depth/stencil buffer). This can happen when Nanite renders custom depth on platforms with HW that cannot write stencil
	// values per-pixel from a shader.
	bool bSeparateStencilBuffer = false;

	// Actions to use when initially rendering to custom depth / stencil.
	ERenderTargetLoadAction DepthAction = ERenderTargetLoadAction::EClear;
	ERenderTargetLoadAction StencilAction = ERenderTargetLoadAction::EClear;
};
