// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DummyRenderResource.h: Frequently used rendering resources
=============================================================================*/

#pragma once

#include "GlobalShader.h"
#include "HAL/Platform.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "PipelineStateCache.h"
#include "RHI.h"
#include "RHIDefinitions.h"
#include "RenderResource.h"
#include "Serialization/MemoryLayout.h"
#include "Shader.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "Templates/UnrealTemplate.h"

class FPointerTableBase;
class FRDGTexture;


/** The vertex data used to filter a texture. */
struct FFilterVertex
{
public:
	FVector4f Position;
	FVector2f UV;
};

/** The filter vertex declaration resource type. */
class FFilterVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FFilterVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof(FFilterVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FFilterVertex, Position), VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FFilterVertex, UV), VET_Float2, 1, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern RENDERCORE_API TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

/** The empty vertex declaration resource type. */
class FEmptyVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FEmptyVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern RENDERCORE_API TGlobalResource<FEmptyVertexDeclaration> GEmptyVertexDeclaration;

/**
 * Static vertex and index buffer used for 2D screen rectangles.
 */
class FScreenRectangleVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() override;
};

extern RENDERCORE_API TGlobalResource<FScreenRectangleVertexBuffer> GScreenRectangleVertexBuffer;


class FScreenRectangleIndexBuffer : public FIndexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() override;
};

extern RENDERCORE_API TGlobalResource<FScreenRectangleIndexBuffer> GScreenRectangleIndexBuffer;


/** Vertex shader to draw a screen quad that works on all platforms. Does not have any shader parameters.
 * The pixel shader should just use SV_Position. */
class RENDERCORE_API FScreenVertexShaderVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenVertexShaderVS);
	SHADER_USE_PARAMETER_STRUCT(FScreenVertexShaderVS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()
};

/** Pixel shader to copy pixels from src to dst performing a format change that works on all platforms. */
class RENDERCORE_API FCopyRectPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCopyRectPS);
	SHADER_USE_PARAMETER_STRUCT(FCopyRectPS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return true;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
