// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenSceneLighting.h
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "SceneView.h"
#include "SceneRendering.h"
#include "RendererPrivateUtils.h"
#include "Lumen.h"
#include "LumenSceneRendering.h"

class FLumenCardRenderer;
class FLumenLight;
class FLumenCardTracingInputs;

BEGIN_SHADER_PARAMETER_STRUCT(FLumenCardScatterParameters, )
	RDG_BUFFER_ACCESS(DrawIndirectArgs, ERHIAccess::IndirectArgs)
	RDG_BUFFER_ACCESS(DispatchIndirectArgs, ERHIAccess::IndirectArgs)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, QuadAllocator)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, QuadData)
	SHADER_PARAMETER(uint32, MaxQuadsPerScatterInstance)
END_SHADER_PARAMETER_STRUCT()

struct FCardCaptureAtlas
{
	FIntPoint Size;
	FRDGTextureRef Albedo;
	FRDGTextureRef Normal;
	FRDGTextureRef Emissive;
	FRDGTextureRef DepthStencil;
};

class FLumenCardUpdateContext
{
public:
	enum EIndirectArgOffset
	{
		ThreadPerPage = 0 * sizeof(FRHIDispatchIndirectParameters),
		ThreadPerTile = 1 * sizeof(FRHIDispatchIndirectParameters),
		MAX = 2,
	};

	FRDGBufferRef CardPageIndexAllocator;
	FRDGBufferRef CardPageIndexData;
	FRDGBufferRef DrawCardPageIndicesIndirectArgs;
	FRDGBufferRef DispatchCardPageIndicesIndirectArgs;

	FIntPoint UpdateAtlasSize;
	uint32 MaxUpdateTiles;
	uint32 UpdateFactor;
};

class FRasterizeToCardsVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterizeToCardsVS);
	SHADER_USE_PARAMETER_STRUCT(FRasterizeToCardsVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		RDG_BUFFER_ACCESS(DrawIndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageIndexData)
		SHADER_PARAMETER(FVector2f, IndirectLightingAtlasSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

template<typename VertexShaderType, typename PixelShaderType, typename PassParametersType, typename SetParametersLambdaType>
void DrawQuadsToAtlas(
	FIntPoint ViewportSize,
	TShaderRefBase<VertexShaderType, FShaderMapPointerTable> VertexShader,
	TShaderRefBase<PixelShaderType, FShaderMapPointerTable> PixelShader,
	const PassParametersType* PassParameters,
	const FGlobalShaderMap* GlobalShaderMap,
	FRHIBlendState* BlendState,
	FRHICommandList& RHICmdList,
	SetParametersLambdaType&& SetParametersLambda,
	FRDGBufferRef DrawIndirectArgs,
	uint32 DrawIndirectArgOffset)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	RHICmdList.SetViewport(0, 0, 0.0f, ViewportSize.X, ViewportSize.Y, 1.0f);

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = BlendState;

	GraphicsPSOInit.PrimitiveType = GRHISupportsRectTopology ? PT_RectList : PT_TriangleList;

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);
	SetParametersLambda(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

	RHICmdList.DrawPrimitiveIndirect(DrawIndirectArgs->GetIndirectRHICallBuffer(), DrawIndirectArgOffset);
}

class FClearLumenCardsPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearLumenCardsPS);
	SHADER_USE_PARAMETER_STRUCT(FClearLumenCardsPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
	END_SHADER_PARAMETER_STRUCT()

	class FNumTargets : SHADER_PERMUTATION_RANGE_INT("NUM_TARGETS", 1, 2);
	using FPermutationDomain = TShaderPermutationDomain<FNumTargets>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

class FCopyCardCaptureLightingToAtlasPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCopyCardCaptureLightingToAtlasPS);
	SHADER_USE_PARAMETER_STRUCT(FCopyCardCaptureLightingToAtlasPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(float, DiffuseColorBoost)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AlbedoCardCaptureAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EmissiveCardCaptureAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DirectLightingCardCaptureAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RadiosityCardCaptureAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RadiosityNumFramesAccumulatedCardCaptureAtlas)
	END_SHADER_PARAMETER_STRUCT()

	class FIndirectLighting : SHADER_PERMUTATION_BOOL("INDIRECT_LIGHTING");
	class FResample : SHADER_PERMUTATION_BOOL("RESAMPLE");
	using FPermutationDomain = TShaderPermutationDomain<FIndirectLighting, FResample>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

// Must match LIGHT_TYPE_* in LumenSceneDirectLighting.usf
enum class ELumenLightType
{
	Directional,
	Point,
	Spot,
	Rect,

	MAX
};

struct FLumenShadowSetup
{
	int32 VirtualShadowMapId;
	const FProjectedShadowInfo* DenseShadowMap;
};

FLumenShadowSetup GetShadowForLumenDirectLighting(FVisibleLightInfo& VisibleLightInfo);

void TraceLumenHardwareRayTracedDirectLightingShadows(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	int32 ViewIndex,
	const FLumenCardTracingInputs& TracingInputs,
	FRDGBufferRef ShadowTraceIndirectArgs,
	FRDGBufferRef ShadowTraceAllocator,
	FRDGBufferRef ShadowTraces,
	FRDGBufferRef LightTileAllocator,
	FRDGBufferRef LightTiles,
	FRDGBufferRef LumenPackedLights,
	FRDGBufferUAVRef ShadowMaskTilesUAV);

enum class ELumenDispatchCardTilesIndirectArgsOffset
{
	OneThreadPerCardTile = 0 * sizeof(FRHIDispatchIndirectParameters),
	OneGroupPerCardTile = 1 * sizeof(FRHIDispatchIndirectParameters),
	Num = 2
};

struct FLumenCardTileUpdateContext
{
	FRDGBufferRef CardTileAllocator;
	FRDGBufferRef CardTiles;
	FRDGBufferRef DispatchCardTilesIndirectArgs;
};

namespace Lumen
{
	void SetDirectLightingDeferredLightUniformBuffer(
		const FViewInfo& View,
		const FLightSceneInfo* LightSceneInfo,
		TUniformBufferBinding<FDeferredLightUniformStruct>& UniformBuffer);

	void CombineLumenSceneLighting(
		FScene* Scene,
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FLumenCardTracingInputs& TracingInputs,
		const FLumenCardUpdateContext& CardUpdateContext,
		const FLumenCardTileUpdateContext& CardTileUpdateContext,
		ERDGPassFlags ComputePassFlags);

	void BuildCardUpdateContext(
		FRDGBuilder& GraphBuilder,
		const FLumenSceneData& LumenSceneData,
		const TArray<FViewInfo>& Views,
		const FLumenSceneFrameTemporaries& FrameTemporaries,
		FLumenCardUpdateContext& DirectLightingCardUpdateContext,
		FLumenCardUpdateContext& IndirectLightingCardUpdateContext,
		ERDGPassFlags ComputePassFlags);

	void SpliceCardPagesIntoTiles(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* GloablShaderMap,
		const FLumenCardUpdateContext& CardUpdateContext,
		const TRDGUniformBufferRef<FLumenCardScene>& LumenCardSceneUniformBuffer,
		FLumenCardTileUpdateContext& OutCardTileUpdateContext,
		ERDGPassFlags ComputePassFlags);

	inline EPixelFormat GetDirectLightingAtlasFormat() { return PF_FloatR11G11B10; }
	inline EPixelFormat GetIndirectLightingAtlasFormat() { return PF_FloatR11G11B10; }
	inline EPixelFormat GetNumFramesAccumulatedAtlasFormat() { return PF_R8; }
};

namespace LumenSceneDirectLighting
{
	float GetShadowMapSamplingBias();
	float GetVirtualShadowMapSamplingBias();
	float GetMeshSDFShadowRayBias();
	float GetHeightfieldShadowRayBias();
	float GetGlobalSDFShadowRayBias();
	float GetHardwareRayTracingShadowRayBias();
	bool UseVirtualShadowMaps();
	bool AllowShadowMaps(const FEngineShowFlags& EngineShowFlags);
}