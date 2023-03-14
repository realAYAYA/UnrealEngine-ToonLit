// Copyright Epic Games, Inc. All Rights Reserved.


#include "ClearReplacementShaders.h"
#include "ShaderParameterUtils.h"
#include "RendererInterface.h"

IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementVS									, SF_Vertex);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementVS_Bounds							, SF_Vertex);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementVS_Depth							, SF_Vertex);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementPS									, SF_Pixel);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementPS_128								, SF_Pixel);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementPS_Zero							, SF_Pixel);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Buffer_Uint_Bounds				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Buffer_Uint4_Bounds				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Buffer_Uint4					, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Buffer_Float_Bounds				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Buffer_Float4_Bounds			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_LargeBuffer_Uint_Bounds			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_LargeBuffer_Uint4_Bounds		, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_LargeBuffer_Uint4				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_LargeBuffer_Float_Bounds		, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_LargeBuffer_Float4_Bounds		, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_StructuredBuffer_Uint_Bounds	, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_StructuredBuffer_Uint4_Bounds	, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_StructuredBuffer_Uint4			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_StructuredBuffer_Float_Bounds	, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_StructuredBuffer_Float4_Bounds	, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture3D_Float4_Bounds			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2DArray_Float4_Bounds	, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2D_Float4_Bounds			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Buffer_Uint_Zero				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_StructuredBuffer_Uint_Zero		, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2DArray_Uint_Zero		, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Buffer_Uint						, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_LargeBuffer_Uint				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_StructuredBuffer_Uint			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2DArray_Uint				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture3D_Float4				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2D_Float4				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2DArray_Float4			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture3D_Uint4					, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2D_Uint4					, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2DArray_Uint4			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture3D_Uint4_Bounds			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2D_Uint4_Bounds			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2DArray_Uint4_Bounds		, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Buffer_Sint4_Bounds				, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_LargeBuffer_Sint4_Bounds		, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_StructuredBuffer_Sint4_Bounds	, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture3D_Sint4_Bounds			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2D_Sint4_Bounds			, SF_Compute);
IMPLEMENT_SHADER_TYPE4_WITH_TEMPLATE_PREFIX(template<>, RENDERCORE_API, FClearReplacementCS_Texture2DArray_Sint4_Bounds		, SF_Compute);

void CreateClearReplacementShaders()
{
#if !WITH_EDITOR	// in editor, this function overall has a lesser sence due to the ability to switch the preview modes on the fly and recompile the shaders, which both can execute the code on GT while RT is already created.
	// if RHI supports MT shader creation, then we don't care to init here
	ensureMsgf(!GRHISupportsMultithreadedShaderCreation, TEXT("InitClearReplacementShaders() is called while GRHISupportsMultithreadedShaderCreation is true. This is an unnecessary call."));
	ensureMsgf(IsInRenderingThread() || GRHISupportsMultithreadedShaderCreation, TEXT("InitClearReplacementShaders() is expected to be called from the render thread if GRHISupportsMultithreadedShaderCreation is false."));

#define UE_CREATE_SHADER(TShaderType) \
	{ \
		TShaderMapRef<TShaderType> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel)); \
		FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader(); \
	}

	UE_CREATE_SHADER(FClearReplacementCS_Buffer_Uint_Bounds);

	UE_CREATE_SHADER(FClearReplacementCS_Buffer_Float_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Buffer_Float4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_LargeBuffer_Uint_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_LargeBuffer_Float_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_LargeBuffer_Float4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_StructuredBuffer_Uint_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_StructuredBuffer_Float_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_StructuredBuffer_Float4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture3D_Float4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2D_Float4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2DArray_Float4_Bounds);

	// Compute shaders for clearing each resource type. No bounds checks enabled.
	UE_CREATE_SHADER(FClearReplacementCS_Buffer_Uint_Zero);
	UE_CREATE_SHADER(FClearReplacementCS_StructuredBuffer_Uint_Zero);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2DArray_Uint_Zero);
	UE_CREATE_SHADER(FClearReplacementCS_Buffer_Uint);
	UE_CREATE_SHADER(FClearReplacementCS_LargeBuffer_Uint);
	UE_CREATE_SHADER(FClearReplacementCS_StructuredBuffer_Uint);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2DArray_Uint);

	UE_CREATE_SHADER(FClearReplacementCS_Texture3D_Float4);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2D_Float4);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2DArray_Float4);

	// Used by ClearUAV_T in ClearQuad.cpp
	UE_CREATE_SHADER(FClearReplacementCS_Texture3D_Uint4);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2D_Uint4);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2DArray_Uint4);
	UE_CREATE_SHADER(FClearReplacementCS_Buffer_Uint4);
	UE_CREATE_SHADER(FClearReplacementCS_LargeBuffer_Uint4);
	UE_CREATE_SHADER(FClearReplacementCS_StructuredBuffer_Uint4);

	UE_CREATE_SHADER(FClearReplacementCS_Buffer_Uint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_LargeBuffer_Uint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_StructuredBuffer_Uint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture3D_Uint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2D_Uint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2DArray_Uint4_Bounds);

	UE_CREATE_SHADER(FClearReplacementCS_Buffer_Sint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_LargeBuffer_Sint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_StructuredBuffer_Sint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture3D_Sint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2D_Sint4_Bounds);
	UE_CREATE_SHADER(FClearReplacementCS_Texture2DArray_Sint4_Bounds);

#undef UE_CREATE_SHADER
#endif
}
