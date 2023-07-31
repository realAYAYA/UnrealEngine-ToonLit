// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalGraphicsPipelineState.h: Metal RHI graphics pipeline state class.
=============================================================================*/

#pragma once

class FMetalGraphicsPipelineState : public FRHIGraphicsPipelineState
{
	friend class FMetalDynamicRHI;

public:
	virtual ~FMetalGraphicsPipelineState();

	FMetalShaderPipeline* GetPipeline();

	/** Cached vertex structure */
	TRefCountPtr<FMetalVertexDeclaration> VertexDeclaration;

	/** Cached shaders */
	TRefCountPtr<FMetalVertexShader> VertexShader;
	TRefCountPtr<FMetalPixelShader> PixelShader;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	TRefCountPtr<FMetalGeometryShader> GeometryShader;
#endif

	/** Cached state objects */
	TRefCountPtr<FMetalDepthStencilState> DepthStencilState;
	TRefCountPtr<FMetalRasterizerState> RasterizerState;

	EPrimitiveType GetPrimitiveType()
	{
		return Initializer.PrimitiveType;
	}

	bool GetDepthBounds() const
	{
		return Initializer.bDepthBounds;
	}

private:
	// This can only be created through the RHI to make sure Compile() is called.
	FMetalGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Init);

	// Compiles the underlying gpu pipeline objects. This must be called before usage.
	bool Compile();

	// Needed to runtime refine shaders currently.
	FGraphicsPipelineStateInitializer Initializer;

	FMetalShaderPipeline* PipelineState;
};
