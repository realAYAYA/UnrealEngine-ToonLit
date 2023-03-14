// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterInfoRendering.h"

#include "EngineModule.h"
#include "LegacyScreenPercentageDriver.h"
#include "RenderCaptureInterface.h"
#include "WaterBodyActor.h"
#include "WaterBodySceneProxy.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "SceneCaptureRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "Math/OrthoMatrix.h"
#include "GameFramework/WorldSettings.h"
#include "WaterZoneActor.h"
#include "Rendering/NaniteStreamingManager.h"
#include "SceneViewExtension.h"
#include "LandscapeRender.h"
#include "LandscapeModule.h"

static int32 RenderCaptureNextWaterInfoDraws = 0;
static FAutoConsoleVariableRef CVarRenderCaptureNextWaterInfoDraws(
	TEXT("r.Water.WaterInfo.RenderCaptureNextWaterInfoDraws"),
	RenderCaptureNextWaterInfoDraws,
	TEXT("Enable capturing of the water info texture for the next N draws"));

namespace UE::WaterInfo
{

struct FUpdateWaterInfoParams
{
	FSceneRenderer* DepthRenderer = nullptr;
	FSceneRenderer* ColorRenderer = nullptr;
	FSceneRenderer* DilationRenderer = nullptr;
	FRenderTarget* RenderTarget = nullptr;
	FTexture* OutputTexture = nullptr;

	FVector WaterZoneExtents;
	FVector2f WaterHeightExtents;
	float GroundZMin;
	float CaptureZ;
	int32 VelocityBlurRadius;
};


// ---------------------------------------------------------------------------------------------------------------------

/** A pixel shader for capturing a component of the rendered scene for a scene capture.*/
class FWaterInfoMergePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterInfoMergePS);
	SHADER_USE_PARAMETER_STRUCT(FWaterInfoMergePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DepthTextureSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ColorTextureSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DilationTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DilationTextureSampler)
		SHADER_PARAMETER(FVector2f, WaterHeightExtents)
		SHADER_PARAMETER(float, GroundZMin)
		SHADER_PARAMETER(float, CaptureZ)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Water info merge unconditionally requires a 128 bit render target. Some platforms require explicitly enabling this output mode.
		bool bPlatformRequiresExplicit128bitRT = FDataDrivenShaderPlatformInfo::GetRequiresExplicit128bitRT(Parameters.Platform);
		if (bPlatformRequiresExplicit128bitRT)
		{
			OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FWaterInfoMergePS, "/Plugin/Water/Private/WaterInfoMerge.usf", "Main", SF_Pixel);

static void MergeWaterInfoAndDepth(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FSceneViewFamily& ViewFamily,
	FViewInfo& View,
	FRDGTextureRef OutputTexture,
	FRDGTextureRef DepthTexture,
	FRDGTextureRef ColorTexture,
	FRDGTextureRef DilationTexture,
	const FUpdateWaterInfoParams& Params)
{
	RDG_EVENT_SCOPE(GraphBuilder, "WaterInfoDepthMerge");

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	{
		FWaterInfoMergePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterInfoMergePS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);
		PassParameters->SceneTextures = SceneTextures.GetSceneTextureShaderParameters(ViewFamily.GetFeatureLevel());
		PassParameters->DepthTexture = DepthTexture;
		PassParameters->DepthTextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->ColorTexture = ColorTexture;
		PassParameters->ColorTextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->DilationTexture = DilationTexture;
		PassParameters->DilationTextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->CaptureZ = Params.CaptureZ;
		PassParameters->WaterHeightExtents = Params.WaterHeightExtents;
		PassParameters->GroundZMin = Params.GroundZMin;

		TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FWaterInfoMergePS> PixelShader(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("WaterInfoDepthMerge"),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, GraphicsPSOInit, VertexShader, PixelShader, &View](FRHICommandListImmediate& RHICmdList)
			{
				FGraphicsPipelineStateInitializer LocalGraphicsPSOInit = GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(LocalGraphicsPSOInit);
				SetGraphicsPipelineState(RHICmdList, LocalGraphicsPSOInit, 0);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

				DrawRectangle(
					RHICmdList,
					View.ViewRect.Min.X, View.ViewRect.Min.Y,
					View.ViewRect.Width(), View.ViewRect.Height(),
					View.ViewRect.Min.X, View.ViewRect.Min.Y,
					View.ViewRect.Width(), View.ViewRect.Height(),
					View.UnconstrainedViewRect.Size(),
					View.UnconstrainedViewRect.Size(),
					VertexShader,
					EDRF_UseTriangleOptimization);
			});
	}
}

// ---------------------------------------------------------------------------------------------------------------------


/** A pixel shader for capturing a component of the rendered scene for a scene capture.*/
class FWaterInfoFinalizePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterInfoFinalizePS);
	SHADER_USE_PARAMETER_STRUCT(FWaterInfoFinalizePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, WaterInfoTexture)
		SHADER_PARAMETER(FVector2f, WaterHeightExtents)
		SHADER_PARAMETER(float, GroundZMin)
		SHADER_PARAMETER(float, CaptureZ)
		SHADER_PARAMETER(int, BlurRadius)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	class FEnable128BitRT : SHADER_PERMUTATION_BOOL("ENABLE_128_BIT");
	using FPermutationDomain = TShaderPermutationDomain<FEnable128BitRT>;

	static FPermutationDomain GetPermutationVector(bool bUse128BitRT)
	{
		FPermutationDomain PermutationVector;
		PermutationVector.Set<FEnable128BitRT>(bUse128BitRT);
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		bool bPlatformRequiresExplicit128bitRT = FDataDrivenShaderPlatformInfo::GetRequiresExplicit128bitRT(Parameters.Platform);
		return (!PermutationVector.Get<FEnable128BitRT>() || bPlatformRequiresExplicit128bitRT);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FEnable128BitRT>())
		{
			OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FWaterInfoFinalizePS, "/Plugin/Water/Private/WaterInfoFinalize.usf", "Main", SF_Pixel);

static void FinalizeWaterInfo(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FSceneViewFamily& ViewFamily,
	FViewInfo& View,
	FRDGTextureRef WaterInfoTexture,
	FRDGTextureRef OutputTexture,
	const FUpdateWaterInfoParams& Params)
{
	RDG_EVENT_SCOPE(GraphBuilder, "WaterInfoFinalize");

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	const bool bUse128BitRT = PlatformRequires128bitRT(OutputTexture->Desc.Format);
	const FWaterInfoFinalizePS::FPermutationDomain PixelPermutationVector = FWaterInfoFinalizePS::GetPermutationVector(bUse128BitRT);

	{
		FWaterInfoFinalizePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterInfoFinalizePS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);
		PassParameters->SceneTextures = SceneTextures.GetSceneTextureShaderParameters(ViewFamily.GetFeatureLevel());
		PassParameters->WaterInfoTexture = WaterInfoTexture;
		PassParameters->BlurRadius = Params.VelocityBlurRadius;
		PassParameters->CaptureZ = Params.CaptureZ;
		PassParameters->WaterHeightExtents = Params.WaterHeightExtents;
		PassParameters->GroundZMin = Params.GroundZMin;

		TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FWaterInfoFinalizePS> PixelShader(View.ShaderMap, PixelPermutationVector);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("WaterInfoFinalize"),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, GraphicsPSOInit, VertexShader, PixelShader, &View](FRHICommandListImmediate& RHICmdList)
			{
				FGraphicsPipelineStateInitializer LocalGraphicsPSOInit = GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(LocalGraphicsPSOInit);
				SetGraphicsPipelineState(RHICmdList, LocalGraphicsPSOInit, 0);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
				
				DrawRectangle(
					RHICmdList,
					View.ViewRect.Min.X, View.ViewRect.Min.Y,
					View.ViewRect.Width(), View.ViewRect.Height(),
					View.ViewRect.Min.X, View.ViewRect.Min.Y,
					View.ViewRect.Width(), View.ViewRect.Height(),
					View.UnconstrainedViewRect.Size(),
					View.UnconstrainedViewRect.Size(),
					VertexShader,
					EDRF_UseTriangleOptimization);
			});
	}
}
// ---------------------------------------------------------------------------------------------------------------------

static FMatrix BuildOrthoMatrix(float InOrthoWidth, float InOrthoHeight)
{
	check((int32)ERHIZBuffer::IsInverted);

	const FMatrix::FReal OrthoWidth = InOrthoWidth / 2.0f;
	const FMatrix::FReal OrthoHeight = InOrthoHeight / 2.0f;

	const FMatrix::FReal NearPlane = 0.f;
	const FMatrix::FReal FarPlane = UE_FLOAT_HUGE_DISTANCE / 4.0f;

	const FMatrix::FReal ZScale = 1.0f / (FarPlane - NearPlane);
	const FMatrix::FReal ZOffset = 0;

	return FReversedZOrthoMatrix(
		OrthoWidth,
		OrthoHeight,
		ZScale,
		ZOffset
		);
}

// ---------------------------------------------------------------------------------------------------------------------

static void SetWaterBodiesWithinWaterInfoPass(FSceneRenderer* SceneRenderer, EWaterInfoPass InPass)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::SetWaterBodiesWithinWaterInfoPass);
	if (SceneRenderer->Views[0].ShowOnlyPrimitives.IsSet())
	{
		for (FPrimitiveSceneProxy* PrimProxy : SceneRenderer->Scene->PrimitiveSceneProxies)
		{
			if (PrimProxy && SceneRenderer->Views[0].ShowOnlyPrimitives->Contains(PrimProxy->GetPrimitiveComponentId()))
			{
				FWaterBodySceneProxy* WaterProxy = (FWaterBodySceneProxy*)PrimProxy;
				WaterProxy->SetWithinWaterInfoPass(InPass);
			}
		}
	}
}

/**
 * Sets up the landscape LOD override for water info rendering and restores it when the instance goes out of scope.
 */
struct FScopedLandscapeLODOverride
{
public:
	FScopedLandscapeLODOverride(const FUpdateWaterInfoParams& InParams)	
		: Params(InParams)
	{
		if (ILandscapeModule* LandscapeModule = FModuleManager::GetModulePtr<ILandscapeModule>("Landscape"))
		{
			if (TSharedPtr<FLandscapeSceneViewExtension, ESPMode::ThreadSafe> LandscapeViewExtension = LandscapeModule->GetLandscapeSceneViewExtension())
			{
				LandscapeRenderSystems = &LandscapeViewExtension->GetLandscapeRenderSystems();
			}
		}

		SetOptimalLandscapeLODOverrides();
	}

	~FScopedLandscapeLODOverride()
	{
		RestoreLandscapeLODOverrides();
	}

private:
	FScopedLandscapeLODOverride(const FScopedLandscapeLODOverride&) = delete;
	FScopedLandscapeLODOverride& operator=(const FScopedLandscapeLODOverride&) = delete;

	void SetOptimalLandscapeLODOverrides()
	{
		if (!LandscapeRenderSystems)
		{
			return;
		}
		
		// In order to prevent overdrawing the landscape components, we compute the lowest-detailed LOD level which satisfies the pixel coverage of the Water Info texture
		// and force it on all landscape components. This override is set different per Landscape actor in case there are multiple under the same water zone.
		//
		// Ex: If the WaterInfoTexture only has 1 pixel per 100 units, and the highest landscape LOD has 1 vertex per 20 units, we don't need to use the maximum landscape LOD
		// and can force a lower level of detail (in this case LOD2) while still satisfying the resolution of the water info texture.

		const double MinWaterInfoTextureExtent = (double)FMath::Min(Params.OutputTexture->GetSizeX(), Params.OutputTexture->GetSizeY());
		const double MaxWaterZoneExtent = FMath::Max(Params.WaterZoneExtents.X, Params.WaterZoneExtents.Y);
		const double WaterInfoUnitsPerPixel =  MaxWaterZoneExtent / MinWaterInfoTextureExtent;

		for (const TPair<uint32, FLandscapeRenderSystem*>& Pair : *LandscapeRenderSystems)
		{
			FLandscapeRenderSystem* LandscapeRenderSystem = Pair.Value;
			int32 OptimalLODLevel = INDEX_NONE;

			// All components within the same landscape (and thus its render system) should have the same number of quads and the same extent.
			// therefore we can simply find the first component and compute its optimal LOD level.
			for (FLandscapeSectionInfo* LandscapeSectionInfo : LandscapeRenderSystem->SectionInfos)
			{
				if (LandscapeSectionInfo != nullptr)
				{
					const double LandscapeComponentUnitsPerVertex = LandscapeSectionInfo->ComputeSectionResolution();
					if (LandscapeComponentUnitsPerVertex <= 0.f)
					{
						// No section resolution probably means the section is a mesh proxy, which might not have regular units per vertex.
						// Avoid computing optimal LOD in this case.
						continue;
					}

					// Derived from:
					// LandscapeComponentUnitsPerVertex * 2 ^ (LODLevel) <= WaterInfoUnitsPerPixel * 0.5
					OptimalLODLevel = FMath::Max(0, int32(FMath::FloorLog2(WaterInfoUnitsPerPixel / LandscapeComponentUnitsPerVertex)) - 1);

					break;
				}
			}

			LandscapeLODOverridesToRestore.Add(LandscapeRenderSystem, LandscapeRenderSystem->ForcedLODOverride);
			LandscapeRenderSystem->ForcedLODOverride = OptimalLODLevel;
		}
	}

	void RestoreLandscapeLODOverrides()
	{
		if (!LandscapeRenderSystems) return;
		
		for (const TPair<uint32, FLandscapeRenderSystem*>& Pair : *LandscapeRenderSystems)
		{
			FLandscapeRenderSystem* LandscapeRenderSystem = Pair.Value;
			LandscapeRenderSystem->ForcedLODOverride = LandscapeLODOverridesToRestore.FindChecked(LandscapeRenderSystem);
		}
	}

	const FUpdateWaterInfoParams& Params;
	TMap<uint32, FLandscapeRenderSystem*> const* LandscapeRenderSystems = nullptr;
	TMap<FLandscapeRenderSystem*, int8> LandscapeLODOverridesToRestore;
};

static void UpdateWaterInfoRendering_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	const FUpdateWaterInfoParams& Params)
{
	FScopedLandscapeLODOverride ScopedLandscapeLODOverride(Params);

	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();
	FDeferredUpdateResource::UpdateResources(RHICmdList);

	FRenderTarget* RenderTarget = Params.RenderTarget;
	FTexture* OutputTexture = Params.OutputTexture;

	TRefCountPtr<IPooledRenderTarget> ExtractedDepthTexture;
	TRefCountPtr<IPooledRenderTarget> ExtractedColorTexture;

	// Depth-only pass for actors which are considered the ground for water rendering
	{
		FSceneRenderer* DepthRenderer = Params.DepthRenderer;

		DepthRenderer->RenderThreadBegin(RHICmdList);

		SCOPED_DRAW_EVENT(RHICmdList, DepthRendering_RT);

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("WaterInfoDepthRendering"), ERDGBuilderFlags::AllowParallelExecute);

		// We need to execute the pre-render view extensions before we do any view dependent work.
		FSceneRenderer::ViewExtensionPreRender_RenderThread(GraphBuilder, DepthRenderer);
		
		FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, RenderTarget->GetRenderTargetTexture(), TEXT("WaterDepthTarget"));

		FRDGTextureDesc DepthTextureDesc(TargetTexture->Desc);
		FRDGTextureRef DepthTexture = GraphBuilder.CreateTexture(DepthTextureDesc, TEXT("WaterDepthTexture"));

		FViewInfo& View = DepthRenderer->Views[0];

		AddClearRenderTargetPass(GraphBuilder, TargetTexture, FLinearColor::Black, View.UnscaledViewRect);

		View.bDisableQuerySubmissions = true;
		View.bIgnoreExistingQueries = true;

		{
			RDG_RHI_EVENT_SCOPE(GraphBuilder, RenderWaterInfoDepth);
			DepthRenderer->Render(GraphBuilder);
		}
		
		if (DepthRenderer->Scene->GetShadingPath() == EShadingPath::Mobile)
		{
			const FMinimalSceneTextures& SceneTextures = View.GetSceneTextures();
			RDG_EVENT_SCOPE(GraphBuilder, "CaptureSceneColor");
			CopySceneCaptureComponentToTarget(
				GraphBuilder,
				SceneTextures,
				DepthTexture,
				DepthRenderer->ViewFamily,
				DepthRenderer->Views);
		}
		else
		{
			FRHICopyTextureInfo CopyInfo;
			AddCopyTexturePass(GraphBuilder, TargetTexture, DepthTexture, CopyInfo);
		}
		
		// We currently can't have multiple scene renderers run within the same RDGBuilder. Therefore, we must
		// extract the texture to allow it to survive until the water info pass which runs in a later RDG graph (however, still within the same frame).
		GraphBuilder.QueueTextureExtraction(DepthTexture, &ExtractedDepthTexture);

		GraphBuilder.Execute();

		DepthRenderer->RenderThreadEnd(RHICmdList);
	}

	// Render the water bodies' data including flow, zoffset, depth
	{
		FSceneRenderer* ColorRenderer = Params.ColorRenderer;

		ColorRenderer->RenderThreadBegin(RHICmdList);

		SCOPED_DRAW_EVENT(RHICmdList, ColorRendering_RT);

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("WaterInfoColorRendering"), ERDGBuilderFlags::AllowParallelExecute);

		// We need to execute the pre-render view extensions before we do any view dependent work.
		FSceneRenderer::ViewExtensionPreRender_RenderThread(GraphBuilder, ColorRenderer);

		SetWaterBodiesWithinWaterInfoPass(ColorRenderer, EWaterInfoPass::Color);
		
		FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, RenderTarget->GetRenderTargetTexture(), TEXT("WaterColorTarget"));

		FRDGTextureDesc ColorTextureDesc(TargetTexture->Desc);
		ColorTextureDesc.Format = PF_A32B32G32R32F;
		FRDGTextureRef ColorTexture = GraphBuilder.CreateTexture(ColorTextureDesc, TEXT("WaterColorTexture"));
		
		FViewInfo& View = ColorRenderer->Views[0];

		AddClearRenderTargetPass(GraphBuilder, TargetTexture, FLinearColor::Black, View.UnscaledViewRect);

		View.bDisableQuerySubmissions = true;
		View.bIgnoreExistingQueries = true;

		{
			RDG_RHI_EVENT_SCOPE(GraphBuilder, RenderWaterInfoColor);
			ColorRenderer->Render(GraphBuilder);
		}

		const FMinimalSceneTextures& SceneTextures = View.GetSceneTextures();

		// This CopySceneCaptureComponentToTarget is required on all platforms as it extracts a higher pixel depth texture than the scene render target so we can't just copy out of it
		{
			RDG_EVENT_SCOPE(GraphBuilder, "CaptureSceneColor");
			CopySceneCaptureComponentToTarget(
				GraphBuilder,
				SceneTextures,
				ColorTexture,
				ColorRenderer->ViewFamily,
				ColorRenderer->Views);
		}

		// We currently can't have multiple scene renderers run within the same RDGBuilder. Therefore, we must
		// extract the texture to allow it to survive until the water info pass which runs in a later RDG graph (however, still within the same frame).
		GraphBuilder.QueueTextureExtraction(ColorTexture, &ExtractedColorTexture);

		GraphBuilder.Execute();
		
		ColorRenderer->RenderThreadEnd(RHICmdList);
	}

	// Depth-only pass for water body dilated sections which get composited back into the water info texture at a lower priority than regular water body data
	{
		FSceneRenderer* DilationRenderer = Params.DilationRenderer;

		DilationRenderer->RenderThreadBegin(RHICmdList);

		SCOPED_DRAW_EVENT(RHICmdList, DilationRendering_RT);

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("WaterInfoDilationRendering"), ERDGBuilderFlags::AllowParallelExecute);


		// We need to execute the pre-render view extensions before we do any view dependent work.
		FSceneRenderer::ViewExtensionPreRender_RenderThread(GraphBuilder, DilationRenderer);

		SetWaterBodiesWithinWaterInfoPass(DilationRenderer, EWaterInfoPass::Dilation);

		FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, RenderTarget->GetRenderTargetTexture(), TEXT("WaterDilationTarget"));

		FRDGTextureRef DepthTexture = GraphBuilder.RegisterExternalTexture(ExtractedDepthTexture);
		FRDGTextureRef ColorTexture = GraphBuilder.RegisterExternalTexture(ExtractedColorTexture);

		FViewInfo& View = DilationRenderer->Views[0];

		AddClearRenderTargetPass(GraphBuilder, TargetTexture, FLinearColor::Black, View.UnscaledViewRect);

		View.bDisableQuerySubmissions = true;
		View.bIgnoreExistingQueries = true;

		{
			RDG_RHI_EVENT_SCOPE(GraphBuilder, RenderWaterInfoDilation);
			DilationRenderer->Render(GraphBuilder);
		}

		const FMinimalSceneTextures& SceneTextures = View.GetSceneTextures();
		FRDGTextureDesc TextureDesc(TargetTexture->Desc);
		FRDGTextureRef DilationTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("WaterDilationTexture"));

		if (DilationRenderer->Scene->GetShadingPath() == EShadingPath::Mobile)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "CaptureSceneColor");
			CopySceneCaptureComponentToTarget(
				GraphBuilder,
				SceneTextures,
				DilationTexture,
				DilationRenderer->ViewFamily,
				DilationRenderer->Views);
		}
		else
		{
			FRHICopyTextureInfo CopyInfo;
			AddCopyTexturePass(GraphBuilder, TargetTexture, DilationTexture, CopyInfo);
		}

		FRDGTextureRef MergeTargetTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("WaterInfoMerged"));
		MergeWaterInfoAndDepth(GraphBuilder, SceneTextures, DilationRenderer->ViewFamily, DilationRenderer->Views[0], MergeTargetTexture, DepthTexture, ColorTexture, DilationTexture, Params);

		FRDGTextureRef FinalizedTexture = GraphBuilder.CreateTexture(TargetTexture->Desc, TEXT("WaterInfoFinalized"));
		FinalizeWaterInfo(GraphBuilder, SceneTextures, DilationRenderer->ViewFamily, DilationRenderer->Views[0], MergeTargetTexture, FinalizedTexture, Params);
		
		FRDGTextureRef ShaderResourceTexture = RegisterExternalTexture(GraphBuilder, OutputTexture->TextureRHI, TEXT("WaterInfoResolve"));
		AddCopyTexturePass(GraphBuilder, FinalizedTexture, ShaderResourceTexture);
		GraphBuilder.Execute();

		SetWaterBodiesWithinWaterInfoPass(DilationRenderer, EWaterInfoPass::None);
		DilationRenderer->RenderThreadEnd(RHICmdList);
	}

	RHICmdList.Transition(FRHITransitionInfo(Params.OutputTexture->TextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));
}

struct FCreateWaterInfoSceneRendererParams
{
public:
	FCreateWaterInfoSceneRendererParams(const WaterInfo::FRenderingContext& InContext) : Context(InContext) {}

	const WaterInfo::FRenderingContext& Context;

	FSceneInterface* Scene = nullptr;
	FRenderTarget* RenderTarget = nullptr;

	FIntPoint RenderTargetSize = FIntPoint(EForceInit::ForceInit);
	FMatrix ViewRotationMatrix = FMatrix(EForceInit::ForceInit);
	FVector ViewLocation = FVector::Zero();
	FMatrix ProjectionMatrix = FMatrix(EForceInit::ForceInit);
	ESceneCaptureSource CaptureSource = ESceneCaptureSource::SCS_MAX;
	TSet<FPrimitiveComponentId> ShowOnlyPrimitives;
};

static FSceneRenderer* CreateWaterInfoSceneRenderer(const FCreateWaterInfoSceneRendererParams& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::CreateWaterInfoRenderer);

	check(Params.Scene != nullptr)
	check(Params.RenderTarget != nullptr)
	check(Params.CaptureSource != ESceneCaptureSource::SCS_MAX)

	FEngineShowFlags ShowFlags(ESFIM_Game);
	ShowFlags.NaniteMeshes = 0;
	ShowFlags.Atmosphere = 0;
	ShowFlags.Lighting = 0;
	ShowFlags.Bloom = 0;
	ShowFlags.ScreenPercentage = 0;
	ShowFlags.Translucency = 0;
	ShowFlags.SeparateTranslucency = 0;
	ShowFlags.AntiAliasing = 0;
	ShowFlags.Fog = 0;
	ShowFlags.VolumetricFog = 0;
	ShowFlags.DynamicShadows = 0;
	
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Params.RenderTarget,
		Params.Scene,
		ShowFlags)
		.SetRealtimeUpdate(false)
		.SetResolveScene(false));
	ViewFamily.SceneCaptureSource = Params.CaptureSource;

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Params.RenderTargetSize.X, Params.RenderTargetSize.Y));
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.ViewActor = Params.Context.ZoneToRender;
	ViewInitOptions.ViewRotationMatrix = Params.ViewRotationMatrix;
	ViewInitOptions.ViewOrigin = Params.ViewLocation;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverrideFarClippingPlaneDistance = -1.f;
	ViewInitOptions.SceneViewStateInterface = nullptr;
	ViewInitOptions.ProjectionMatrix = Params.ProjectionMatrix;
	ViewInitOptions.LODDistanceFactor = 0.001f;
	ViewInitOptions.OverlayColor = FLinearColor::Black;
	ViewInitOptions.bIsSceneCapture = true;

	if (ViewFamily.Scene->GetWorld() != nullptr && ViewFamily.Scene->GetWorld()->GetWorldSettings() != nullptr)
	{
		ViewInitOptions.WorldToMetersScale = ViewFamily.Scene->GetWorld()->GetWorldSettings()->WorldToMeters;
	}

	FSceneView* View = new FSceneView(ViewInitOptions);
	View->GPUMask = FRHIGPUMask::All();
	View->bOverrideGPUMask = true;
	View->AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
	View->SetupAntiAliasingMethod();

	View->ShowOnlyPrimitives = Params.ShowOnlyPrimitives;

	ViewFamily.Views.Add(View);

	View->StartFinalPostprocessSettings(Params.ViewLocation);
	View->EndFinalPostprocessSettings(ViewInitOptions);

	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.f));

	ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(Params.Scene));
	for (const FSceneViewExtensionRef& Extension : ViewFamily.ViewExtensions)
	{
		Extension->SetupViewFamily(ViewFamily);
		Extension->SetupView(ViewFamily, *View);
	}

	return FSceneRenderer::CreateSceneRenderer(&ViewFamily, nullptr);
}

void UpdateWaterInfoRendering(
	FSceneInterface* Scene,
	const WaterInfo::FRenderingContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::UpdateWaterInfoRendering);
	
	RenderCaptureInterface::FScopedCapture RenderCapture((RenderCaptureNextWaterInfoDraws != 0), TEXT("RenderWaterInfo"));
	RenderCaptureNextWaterInfoDraws = FMath::Max(0, RenderCaptureNextWaterInfoDraws - 1);

	if (Context.TextureRenderTarget == nullptr || Scene == nullptr)
	{
		return;
	}
	const FVector ZoneExtent = Context.ZoneToRender->GetTessellatedWaterMeshExtent();

	FVector ViewLocation = Context.ZoneToRender->GetTessellatedWaterMeshCenter();
	ViewLocation.Z = Context.CaptureZ;

	// Zone rendering always happens facing towards negative z.
	const FVector LookAt = ViewLocation - FVector(0.f, 0.f, 1.f);
	
	const FIntPoint CaptureExtent(Context.TextureRenderTarget->GetSurfaceWidth(), Context.TextureRenderTarget->GetSurfaceHeight());

	// Initialize the generic parameters which are passed to each of the scene renderers
	FCreateWaterInfoSceneRendererParams CreateSceneRendererParams(Context);
	CreateSceneRendererParams.Scene = Scene;
	CreateSceneRendererParams.RenderTarget = Context.TextureRenderTarget->GameThread_GetRenderTargetResource();
	CreateSceneRendererParams.RenderTargetSize = CaptureExtent;
	CreateSceneRendererParams.ViewLocation = ViewLocation;
	CreateSceneRendererParams.ProjectionMatrix = BuildOrthoMatrix(ZoneExtent.X, ZoneExtent.Y);
	CreateSceneRendererParams.ViewRotationMatrix = FLookAtMatrix(ViewLocation, LookAt, FVector(0.f, -1.f, 0.f));
	CreateSceneRendererParams.ViewRotationMatrix = CreateSceneRendererParams.ViewRotationMatrix.RemoveTranslation();
	CreateSceneRendererParams.ViewRotationMatrix.RemoveScaling();

	TSet<FPrimitiveComponentId> ComponentsToRenderInDepthPass;
	if (Context.GroundActors.Num() > 0)
	{
		ComponentsToRenderInDepthPass.Reserve(Context.GroundActors.Num());
		for (TWeakObjectPtr<AActor> GroundActor : Context.GroundActors)
		{
			if (GroundActor.IsValid())
			{
				TInlineComponentArray<UPrimitiveComponent*> PrimComps(GroundActor.Get());
				for (UPrimitiveComponent* PrimComp : PrimComps)
				{
					if (PrimComp)
					{
						ComponentsToRenderInDepthPass.Add(PrimComp->ComponentId);
					}
				}
			}
		}
	}
	CreateSceneRendererParams.CaptureSource = SCS_DeviceDepth;
	CreateSceneRendererParams.ShowOnlyPrimitives = MoveTemp(ComponentsToRenderInDepthPass);
	FSceneRenderer* DepthRenderer = CreateWaterInfoSceneRenderer(CreateSceneRendererParams);
	
	TSet<FPrimitiveComponentId> ComponentsToRenderInColorAndDilationPass;
	if (Context.WaterBodies.Num() > 0)
	{
		ComponentsToRenderInColorAndDilationPass.Reserve(Context.WaterBodies.Num());
		for (const UWaterBodyComponent* WaterBodyToRender : Context.WaterBodies)
		{
			ComponentsToRenderInColorAndDilationPass.Add(WaterBodyToRender->ComponentId);
		}
	}
	CreateSceneRendererParams.CaptureSource = SCS_SceneColorSceneDepth;
	CreateSceneRendererParams.ShowOnlyPrimitives = MoveTemp(ComponentsToRenderInColorAndDilationPass);
	FSceneRenderer* ColorRenderer = CreateWaterInfoSceneRenderer(CreateSceneRendererParams);

	CreateSceneRendererParams.CaptureSource = SCS_DeviceDepth;
	FSceneRenderer* DilationRenderer = CreateWaterInfoSceneRenderer(CreateSceneRendererParams);

	FTextureRenderTargetResource* TextureRenderTargetResource = Context.TextureRenderTarget->GameThread_GetRenderTargetResource();

	FUpdateWaterInfoParams Params;
	Params.DepthRenderer = DepthRenderer;
	Params.ColorRenderer = ColorRenderer;
	Params.DilationRenderer = DilationRenderer;
	Params.RenderTarget = TextureRenderTargetResource;
	Params.OutputTexture = TextureRenderTargetResource;
	Params.CaptureZ = ViewLocation.Z;
	Params.WaterHeightExtents = Context.ZoneToRender->GetWaterHeightExtents();
	Params.GroundZMin = Context.ZoneToRender->GetGroundZMin();
	Params.VelocityBlurRadius = Context.ZoneToRender->GetVelocityBlurRadius();
	Params.WaterZoneExtents = ZoneExtent;

	ENQUEUE_RENDER_COMMAND(WaterInfoCommand)(
	[Params, ZoneName = Context.ZoneToRender->GetActorNameOrLabel()](FRHICommandListImmediate& RHICmdList)
		{
			SCOPED_DRAW_EVENTF(RHICmdList, WaterZoneInfoRendering_RT, TEXT("RenderWaterInfo_%s"), *ZoneName);

			UpdateWaterInfoRendering_RenderThread(RHICmdList, Params);
		});
}

} // namespace WaterInfo