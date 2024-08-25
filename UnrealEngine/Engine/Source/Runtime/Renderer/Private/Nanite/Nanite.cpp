// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nanite.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "PixelShaderUtils.h"
#include "ShaderPrintParameters.h"
#include "ShadowRendering.h"
#include "Rendering/NaniteStreamingManager.h"
#include "NaniteVisualizationData.h"
#include "NaniteShading.h"
#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"

#define NUM_PRINT_STATS_PASSES 5

int32 GNaniteShowStats = 0;
FAutoConsoleVariableRef CVarNaniteShowStats(
	TEXT("r.Nanite.ShowStats"),
	GNaniteShowStats,
	TEXT("")
);

FString GNaniteStatsFilter;
FAutoConsoleVariableRef CVarNaniteStatsFilter(
	TEXT("r.Nanite.StatsFilter"),
	GNaniteStatsFilter,
	TEXT("Sets the name of a specific Nanite raster pass to capture stats from - enumerate available filters with `NaniteStats List` cmd."),
	ECVF_RenderThreadSafe
);

extern TAutoConsoleVariable<int32> CVarNaniteShadows;

bool bNaniteListStatFilters = false;

void NaniteStatsFilterExec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	check(IsInGameThread());

	FlushRenderingCommands();

	uint32 ParameterCount = 0;

	// Convenience, force on Nanite debug/stats and also shader printing.
	GNaniteShowStats = 1;
	ShaderPrint::SetEnabled(true);

	// parse parameters
	for (;;)
	{
		FString Parameter = FParse::Token(Cmd, 0);

		if (Parameter.IsEmpty())
		{
			break;
		}

		if (Parameter == TEXT("list"))
		{
			// We don't have access to all the scene data here, so we'll set a flag
			// to print out every filter comparison for the next frame.
			bNaniteListStatFilters = true;
		}
		else if (Parameter == TEXT("primary"))
		{
			// Empty filter name denotes the primary raster view.
			ParameterCount = 0;
			break;
		}
		else if (Parameter == TEXT("off"))
		{
			// disable stats
			GNaniteShowStats = 0;
			return;
		}
		else
		{
			GNaniteStatsFilter = Parameter;
		}

		++ParameterCount;
	}

	if (!ParameterCount)
	{
		// Default to showing stats for the primary view
		GNaniteStatsFilter.Empty();
	}
}

class FEmitShadowMapPS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitShadowMapPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitShadowMapPS, FNaniteGlobalShader);

	class FDepthOutputTypeDim : SHADER_PERMUTATION_INT("DEPTH_OUTPUT_TYPE", 3);
	using FPermutationDomain = TShaderPermutationDomain< FDepthOutputTypeDim >;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		FVirtualShadowMapArray::SetShaderDefines( OutEnvironment );
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER( FIntPoint, SourceOffset )
		SHADER_PARAMETER( float, ViewToClip22 )
		SHADER_PARAMETER( float, DepthBias )
		SHADER_PARAMETER( uint32, ShadowMapID )
		SHADER_PARAMETER_RDG_TEXTURE( Texture2D<uint>,	DepthBuffer )
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FEmitShadowMapPS, "/Engine/Private/Nanite/NaniteEmitShadow.usf", "EmitShadowMapPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FEmitCubemapShadowParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, DepthBuffer)
	SHADER_PARAMETER( uint32, CubemapFaceIndex )
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FEmitCubemapShadowVS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitCubemapShadowVS);
	SHADER_USE_PARAMETER_STRUCT(FEmitCubemapShadowVS, FNaniteGlobalShader);

	class FUseGeometryShader : SHADER_PERMUTATION_BOOL("USE_GEOMETRY_SHADER");
	using FPermutationDomain = TShaderPermutationDomain<FUseGeometryShader>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		if (PermutationVector.Get<FUseGeometryShader>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_VertexToGeometryShader);
		}
	}

	using FParameters = FEmitCubemapShadowParameters;
};
IMPLEMENT_GLOBAL_SHADER(FEmitCubemapShadowVS, "/Engine/Private/Nanite/NaniteEmitShadow.usf", "EmitCubemapShadowVS", SF_Vertex);

class FEmitCubemapShadowGS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitCubemapShadowGS);
	SHADER_USE_PARAMETER_STRUCT(FEmitCubemapShadowGS, FNaniteGlobalShader);

	using FParameters = FEmitCubemapShadowParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return RHISupportsGeometryShaders(Parameters.Platform) && DoesPlatformSupportNanite(Parameters.Platform);
	}
	
	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_GEOMETRY_SHADER"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FEmitCubemapShadowGS, "/Engine/Private/Nanite/NaniteEmitShadow.usf", "EmitCubemapShadowGS", SF_Geometry);

class FEmitCubemapShadowPS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitCubemapShadowPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitCubemapShadowPS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
	}
	
	using FParameters = FEmitCubemapShadowParameters;
};
IMPLEMENT_GLOBAL_SHADER(FEmitCubemapShadowPS, "/Engine/Private/Nanite/NaniteEmitShadow.usf", "EmitCubemapShadowPS", SF_Pixel);

// Gather shading stats
class FCalculateShadingStatsCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateShadingStatsCS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateShadingStatsCS, FNaniteGlobalShader);

	class FLegacyCullingDim : SHADER_PERMUTATION_BOOL("LEGACY_CULLING");
	using FPermutationDomain = TShaderPermutationDomain<FLegacyCullingDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CALCULATE_STATS"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, RenderFlags)
		SHADER_PARAMETER(uint32, NumShadingBins)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteStats>, OutStatsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ShadingBinData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FNaniteShadingBinStats>, ShadingBinStats)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, MaterialIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ShadingBinArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCalculateShadingStatsCS, "/Engine/Private/Nanite/NanitePrintStats.usf", "CalculateShadingStats", SF_Compute);

class FPrintStatsCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FPrintStatsCS );
	SHADER_USE_PARAMETER_STRUCT( FPrintStatsCS, FNaniteGlobalShader);

	class FTwoPassCullingDim : SHADER_PERMUTATION_BOOL( "TWO_PASS_CULLING" );
	class FPassDim : SHADER_PERMUTATION_INT("PRINT_PASS", NUM_PRINT_STATS_PASSES);
	using FPermutationDomain = TShaderPermutationDomain<FTwoPassCullingDim, FPassDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_PRINT_STATS"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER(uint32, PackedClusterSize)
		SHADER_PARAMETER(uint32, RenderFlags)
		SHADER_PARAMETER(uint32, DebugFlags)

		SHADER_PARAMETER_STRUCT_INCLUDE( ShaderPrint::FShaderParameters, ShaderPrintStruct )

		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer<FNaniteStats>, InStatsBuffer )

		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, MainPassRasterizeArgsSWHW )
		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, PostPassRasterizeArgsSWHW )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FPrintStatsCS, "/Engine/Private/Nanite/NanitePrintStats.usf", "PrintStats", SF_Compute);

namespace Nanite
{

FString GetFilterNameForLight(const FLightSceneProxy* LightProxy)
{
	FString LightFilterName;
	{
		FString FullLevelName = LightProxy->GetLevelName().ToString();
		const int32 LastSlashIndex = FullLevelName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		if (LastSlashIndex != INDEX_NONE)
		{
			FullLevelName.MidInline(LastSlashIndex + 1, FullLevelName.Len() - (LastSlashIndex + 1), EAllowShrinking::No);
		}

		LightFilterName = FullLevelName + TEXT(".") + LightProxy->GetOwnerNameOrLabel();
	}

	return LightFilterName;
}

bool IsStatFilterActive(const FString& FilterName)
{
	if (GNaniteShowStats == 0)
	{
		// Stats are disabled, do nothing.
		return false;
	}

	return (GNaniteStatsFilter == FilterName);
}

bool IsStatFilterActiveForLight(const FLightSceneProxy* LightProxy)
{
	if (GNaniteShowStats == 0)
	{
		return false;
	}

	const FString LightFilterName = Nanite::GetFilterNameForLight(LightProxy);
	return IsStatFilterActive(LightFilterName);
}

void ListStatFilters(FSceneRenderer* SceneRenderer)
{
	if (bNaniteListStatFilters && SceneRenderer)
	{
		UE_LOG(LogNanite, Warning, TEXT("** Available Filters **"));

		// Primary view is always available.
		UE_LOG(LogNanite, Warning, TEXT("Primary"));

		const bool bListShadows = CVarNaniteShadows.GetValueOnRenderThread() != 0;

		// Virtual shadow maps
		if (bListShadows)
		{
			const auto& VirtualShadowMaps = SceneRenderer->SortedShadowsForShadowDepthPass.VirtualShadowMapShadows;
			const auto& VirtualShadowClipmaps = SceneRenderer->SortedShadowsForShadowDepthPass.VirtualShadowMapClipmaps;

			if (SceneRenderer->VirtualShadowMapArray.GetNumShadowMaps() > 0)
			{
				UE_LOG(LogNanite, Warning, TEXT("VirtualShadowMaps"));
			}
		}
		
		// Shadow map atlases
		if (bListShadows)
		{
			const auto& ShadowMapAtlases = SceneRenderer->SortedShadowsForShadowDepthPass.ShadowMapAtlases;
			for (int32 AtlasIndex = 0; AtlasIndex < ShadowMapAtlases.Num(); AtlasIndex++)
			{
				UE_LOG(LogNanite, Warning, TEXT("ShadowAtlas%d"), AtlasIndex);
			}
		}

		// Shadow cube maps
		if (bListShadows)
		{
			const auto& ShadowCubeMaps = SceneRenderer->SortedShadowsForShadowDepthPass.ShadowMapCubemaps;
			for (int32 CubemapIndex = 0; CubemapIndex < ShadowCubeMaps.Num(); CubemapIndex++)
			{
				const FSortedShadowMapAtlas& ShadowMap = ShadowCubeMaps[CubemapIndex];
				check(ShadowMap.Shadows.Num() == 1);
				FProjectedShadowInfo* ProjectedShadowInfo = ShadowMap.Shadows[0];

				if (ProjectedShadowInfo->bNaniteGeometry && ProjectedShadowInfo->CacheMode != SDCM_MovablePrimitivesOnly)
				{
					// Get the base light filter name.
					FString CubeFilterName = GetFilterNameForLight(ProjectedShadowInfo->GetLightSceneInfo().Proxy);
					CubeFilterName.Append(TEXT("_Face_"));

					for (int32 CubemapFaceIndex = 0; CubemapFaceIndex < 6; CubemapFaceIndex++)
					{
						FString CubeFaceFilterName = CubeFilterName;
						CubeFaceFilterName.AppendInt(CubemapFaceIndex);
						UE_LOG(LogNanite, Warning, TEXT("Shadow Cube Map: %s"), *CubeFaceFilterName);
					}
				}
			}
		}
	}

	bNaniteListStatFilters = false;
}

void ExtractShadingDebug(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGBufferRef MaterialIndirectArgs,
	const FShadeBinning& ShadeBinning,
	uint32 NumShadingBins
)
{
	LLM_SCOPE_BYTAG(Nanite);

	const FNaniteVisualizationData& VisualizationData = GetNaniteVisualizationData();
	if (VisualizationData.IsActive())
	{
		FRDGBufferRef ShadingBinData = nullptr;
		if (ShadeBinning.ShadingBinData)
		{
			ShadingBinData = ShadeBinning.ShadingBinData;
		}
		else
		{
			ShadingBinData = GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u);
		}

		FRDGTextureRef FastTileVis = nullptr;
		if (ShadeBinning.FastClearVisualize)
		{
			FastTileVis = ShadeBinning.FastClearVisualize;
		}
		else
		{
			FastTileVis = GSystemTextures.GetBlackAlphaOneDummy(GraphBuilder);
		}

		Nanite::GGlobalResources.GetShadingBinDataBufferRef() = GraphBuilder.ConvertToExternalBuffer(ShadingBinData);
		Nanite::GGlobalResources.GetFastClearTileVisRef() = GraphBuilder.ConvertToExternalTexture(FastTileVis);
	}

	if (GNaniteShowStats != 0 && Nanite::GGlobalResources.GetStatsBufferRef())
	{
		const bool bShadeBinning = ShadeBinning.ShadingBinArgs != nullptr;
		FCalculateShadingStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCalculateShadingStatsCS::FParameters>();

		PassParameters->RenderFlags = Nanite::GGlobalResources.StatsRenderFlags;

		PassParameters->OutStatsBuffer = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(Nanite::GGlobalResources.GetStatsBufferRef()));

		PassParameters->NumShadingBins = NumShadingBins;

		if (bShadeBinning)
		{
			PassParameters->ShadingBinData  = GraphBuilder.CreateSRV(ShadeBinning.ShadingBinData);
			PassParameters->ShadingBinStats = GraphBuilder.CreateSRV(ShadeBinning.ShadingBinStats);
			PassParameters->ShadingBinArgs = GraphBuilder.CreateSRV(ShadeBinning.ShadingBinArgs);
		}
		else
		{
			PassParameters->ShadingBinData = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u));
			PassParameters->ShadingBinStats = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FNaniteShadingBinStats>(GraphBuilder));
			PassParameters->MaterialIndirectArgs = GraphBuilder.CreateSRV(MaterialIndirectArgs);
		}

		FCalculateShadingStatsCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCalculateShadingStatsCS::FLegacyCullingDim>(!bShadeBinning);
		auto ComputeShader = View.ShaderMap->GetShader<FCalculateShadingStatsCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CalculateShadingStatsArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1)
		);
	}
}

void PrintStats(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View)
{
	LLM_SCOPE_BYTAG(Nanite);

	// Print stats
	if (GNaniteShowStats != 0 && Nanite::GGlobalResources.GetStatsBufferRef())
	{
		auto& MainPassBuffers = Nanite::GGlobalResources.GetMainPassBuffers();
		auto& PostPassBuffers = Nanite::GGlobalResources.GetPostPassBuffers();

		// Shader compilers have a hard time handling the size of the full PrintStats shader, so we split it into multiple passes.
		// This reduces the FXC compilation time from 2-3 minutes to just a few seconds.
		for (uint32 Pass = 0; Pass < NUM_PRINT_STATS_PASSES; Pass++)
		{
			FPrintStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPrintStatsCS::FParameters>();

			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintStruct);
			PassParameters->PackedClusterSize = sizeof(Nanite::FPackedCluster);

			PassParameters->RenderFlags = Nanite::GGlobalResources.StatsRenderFlags;
			PassParameters->DebugFlags = Nanite::GGlobalResources.StatsDebugFlags;

			PassParameters->InStatsBuffer = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(Nanite::GGlobalResources.GetStatsBufferRef()));

			PassParameters->MainPassRasterizeArgsSWHW = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(MainPassBuffers.StatsRasterizeArgsSWHWBuffer));
			
			const bool bTwoPass = (PostPassBuffers.StatsRasterizeArgsSWHWBuffer != nullptr);
			if( bTwoPass )
			{
				PassParameters->PostPassRasterizeArgsSWHW = GraphBuilder.CreateSRV( GraphBuilder.RegisterExternalBuffer( PostPassBuffers.StatsRasterizeArgsSWHWBuffer ) );
			}

			FPrintStatsCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FPrintStatsCS::FTwoPassCullingDim>(bTwoPass);
			PermutationVector.Set<FPrintStatsCS::FPassDim>(Pass);
			auto ComputeShader = View.ShaderMap->GetShader<FPrintStatsCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Print Stats"),
				ComputeShader,
				PassParameters,
				FIntVector(1, 1, 1)
			);
		}
	}
}

void EmitShadowMap(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FRasterContext& RasterContext,
	const FRDGTextureRef DepthBuffer,
	const FIntRect& SourceRect,
	const FIntPoint DestOrigin,
	const FMatrix& ProjectionMatrix,
	float DepthBias,
	bool bOrtho
	)
{
	LLM_SCOPE_BYTAG(Nanite);

	auto* PassParameters = GraphBuilder.AllocParameters< FEmitShadowMapPS::FParameters >();

	PassParameters->SourceOffset = SourceRect.Min - DestOrigin;
	PassParameters->ViewToClip22 = ProjectionMatrix.M[2][2];
	PassParameters->DepthBias = DepthBias;
	
	PassParameters->DepthBuffer = RasterContext.DepthBuffer;
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding( DepthBuffer, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilNop );

	FEmitShadowMapPS::FPermutationDomain PermutationVector;
	PermutationVector.Set< FEmitShadowMapPS::FDepthOutputTypeDim >( bOrtho ? 1 : 2 );

	auto PixelShader = SharedContext.ShaderMap->GetShader< FEmitShadowMapPS >( PermutationVector );

	FIntRect DestRect;
	DestRect.Min = DestOrigin;
	DestRect.Max = DestRect.Min + SourceRect.Max - SourceRect.Min;
	
	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		SharedContext.ShaderMap,
		RDG_EVENT_NAME("EmitShadowMap"),
		PixelShader,
		PassParameters,
		DestRect,
		nullptr,
		nullptr,
		TStaticDepthStencilState<true, CF_LessEqual>::GetRHI()
	);
}

void EmitCubemapShadow(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FRasterContext& RasterContext,
	const FRDGTextureRef CubemapDepthBuffer,
	const FIntRect& ViewRect,
	uint32 CubemapFaceIndex,
	bool bUseGeometryShader
	)
{
	LLM_SCOPE_BYTAG(Nanite);

	FEmitCubemapShadowVS::FPermutationDomain VertexPermutationVector;
	VertexPermutationVector.Set<FEmitCubemapShadowVS::FUseGeometryShader>(bUseGeometryShader);
	TShaderMapRef<FEmitCubemapShadowVS> VertexShader(SharedContext.ShaderMap, VertexPermutationVector);
	TShaderRef<FEmitCubemapShadowGS> GeometryShader;
	TShaderMapRef<FEmitCubemapShadowPS> PixelShader(SharedContext.ShaderMap);

	// VS output of RT array index on D3D11 requires a caps bit. Use GS fallback if set.
	if (bUseGeometryShader)
	{
		GeometryShader = TShaderMapRef<FEmitCubemapShadowGS>(SharedContext.ShaderMap);
	}

	FEmitCubemapShadowParameters* PassParameters = GraphBuilder.AllocParameters<FEmitCubemapShadowParameters>();
	PassParameters->CubemapFaceIndex = CubemapFaceIndex;	
	PassParameters->DepthBuffer = RasterContext.DepthBuffer;
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(CubemapDepthBuffer, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilNop);
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Emit Cubemap Shadow"),
		PassParameters,
		ERDGPassFlags::Raster,
		[PassParameters, VertexShader, GeometryShader, PixelShader, ViewRect, CubemapFaceIndex](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);
						
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			// NOTE: Shadow cubemaps are reverse Z
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNear>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			if (GeometryShader.GetGeometryShader())
			{
				GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
			}

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *PassParameters);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
			if (GeometryShader.GetGeometryShader())
			{
				SetShaderParameters(RHICmdList, GeometryShader, GeometryShader.GetGeometryShader(), *PassParameters);
			}

			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitive(0, 1, 1);
		}
	);
}

} // namespace Nanite
