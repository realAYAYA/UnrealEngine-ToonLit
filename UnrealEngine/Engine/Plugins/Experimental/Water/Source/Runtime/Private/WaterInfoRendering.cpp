// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterInfoRendering.h"

#include "CommonRenderResources.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "EngineUtils.h"
#include "LegacyScreenPercentageDriver.h"
#include "Modules/ModuleManager.h"
#include "RenderCaptureInterface.h"
#include "RHIStaticStates.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SceneCaptureRendering.h"
#include "PostProcess/DrawRectangle.h"
#include "Math/OrthoMatrix.h"
#include "GameFramework/WorldSettings.h"
#include "ScreenRendering.h"
#include "WaterZoneActor.h"
#include "LandscapeRender.h"
#include "LandscapeModule.h"
#include "TextureResource.h"
#include "WaterBodyComponent.h"
#include "WaterBodyInfoMeshComponent.h"
#include "Containers/StridedView.h"

#include "RenderGraphBuilder.h"
#include "SceneRenderTargetParameters.h"

#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "Rendering/CustomRenderPass.h"

static int32 WaterInfoRenderLandscapeMinimumMipLevel = 0;
static FAutoConsoleVariableRef CVarWaterInfoRenderLandscapeMinimumMipLevel(
	TEXT("r.Water.WaterInfo.LandscapeMinimumMipLevel"),
	WaterInfoRenderLandscapeMinimumMipLevel,
	TEXT("Clamps the minimum allowed mip level for the landscape when rendering the water info texture. Used on the lowest end platforms which cannot support rendering all the landscape vertices at the highest LOD."));

namespace UE::WaterInfo
{

struct FUpdateWaterInfoParams
{
	FSceneInterface* Scene = nullptr;
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
		SHADER_PARAMETER(float, UndergroundDilationDepthOffset)
		SHADER_PARAMETER(float, DilationOverwriteMinimumDistance)
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
	const FSceneViewFamily& ViewFamily,
	const FSceneView& View,
	FRDGTextureRef OutputTexture,
	FRDGTextureRef DepthTexture,
	FRDGTextureRef ColorTexture,
	FRDGTextureRef DilationTexture,
	const FUpdateWaterInfoParams& Params)
{
	RDG_EVENT_SCOPE(GraphBuilder, "WaterInfoDepthMerge");

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	{
		static auto* CVarDilationOverwriteMinimumDistance = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Water.WaterInfo.DilationOverwriteMinimumDistance"));
		static auto* CVarUndergroundDilationDepthOffset = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Water.WaterInfo.UndergroundDilationDepthOffset"));

		FWaterInfoMergePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterInfoMergePS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);
		PassParameters->SceneTextures = GetSceneTextureShaderParameters(View);
		PassParameters->DepthTexture = DepthTexture;
		PassParameters->DepthTextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->ColorTexture = ColorTexture;
		PassParameters->ColorTextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->DilationTexture = DilationTexture;
		PassParameters->DilationTextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->CaptureZ = Params.CaptureZ;
		PassParameters->WaterHeightExtents = Params.WaterHeightExtents;
		PassParameters->GroundZMin = Params.GroundZMin;
		PassParameters->DilationOverwriteMinimumDistance = CVarDilationOverwriteMinimumDistance ? CVarDilationOverwriteMinimumDistance->GetValueOnRenderThread() : 128.0f;
		PassParameters->UndergroundDilationDepthOffset = CVarUndergroundDilationDepthOffset ? CVarUndergroundDilationDepthOffset->GetValueOnRenderThread() : 64.0f;

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FWaterInfoMergePS> PixelShader(ShaderMap);

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

				UE::Renderer::PostProcess::DrawRectangle(RHICmdList, VertexShader, View, EDRF_UseTriangleOptimization);
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
		SHADER_PARAMETER(float, WaterZMin)
		SHADER_PARAMETER(float, WaterZMax)
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
	const FSceneViewFamily& ViewFamily,
	const FSceneView& View,
	FRDGTextureRef WaterInfoTexture,
	FRDGTextureRef OutputTexture,
	const FUpdateWaterInfoParams& Params)
{
	RDG_EVENT_SCOPE(GraphBuilder, "WaterInfoFinalize");

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());

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
		PassParameters->SceneTextures = GetSceneTextureShaderParameters(View);
		PassParameters->WaterInfoTexture = WaterInfoTexture;
		PassParameters->WaterZMin = Params.WaterHeightExtents.X;
		PassParameters->WaterZMax = Params.WaterHeightExtents.Y;
		PassParameters->GroundZMin = Params.GroundZMin;
		PassParameters->CaptureZ = Params.CaptureZ;
		PassParameters->BlurRadius = Params.VelocityBlurRadius;

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FWaterInfoFinalizePS> PixelShader(ShaderMap, PixelPermutationVector);

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

				UE::Renderer::PostProcess::DrawRectangle(RHICmdList, VertexShader, View, EDRF_UseTriangleOptimization);
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

struct FEnableWaterInfoMeshesScoped
{
public:
	FEnableWaterInfoMeshesScoped(FSceneInterface* Scene, const FSceneView& View)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::EnableWaterInfoMeshes);
		if (View.ShowOnlyPrimitives.IsSet())
		{
			for (FPrimitiveSceneProxy* PrimProxy : Scene->GetPrimitiveSceneProxies())
			{
				if (PrimProxy && View.ShowOnlyPrimitives->Contains(PrimProxy->GetPrimitiveComponentId()))
				{
					FWaterBodyInfoMeshSceneProxy* WaterProxy = (FWaterBodyInfoMeshSceneProxy*)PrimProxy;
					WaterProxy->SetEnabled(true);
					Proxies.Add(WaterProxy);
				}
			}
		}
	}

	~FEnableWaterInfoMeshesScoped()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::DisableWaterInfoMeshes);
		for (FWaterBodyInfoMeshSceneProxy* Proxy : Proxies)
		{
			Proxy->SetEnabled(false);
		}
	}
private:
	TArray<FWaterBodyInfoMeshSceneProxy*> Proxies;
};

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
					// Double the required landscape resolution to achieve 2 quads per pixel.
					const double LandscapeComponentUnitsPerQuad = 2.0 * LandscapeSectionInfo->ComputeSectionResolution();
					if (LandscapeComponentUnitsPerQuad <= 0.f)
					{
						// No section resolution probably means the section is a mesh proxy, which might not have regular units per vertex.
						// Avoid computing optimal LOD in this case.
						continue;
					}

					// Derived from:
					// (ComponentWorldExtent / WaterInfoWorldspaceExtent) * WaterInfoTextureResolution = (NumComponentQuads / 2 ^ (LodLevel))
					OptimalLODLevel = FMath::Max(WaterInfoRenderLandscapeMinimumMipLevel, FMath::FloorToInt(FMath::Log2(WaterInfoUnitsPerPixel / LandscapeComponentUnitsPerQuad)));

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

			// Ignore landscape systems that were removed within the scope.
			if (int8* ForcedLODOverride = LandscapeLODOverridesToRestore.Find(LandscapeRenderSystem))
			{
				LandscapeRenderSystem->ForcedLODOverride = *ForcedLODOverride;
			}
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

	static bool bIsUpdatingWaterInfo = false;
	check(bIsUpdatingWaterInfo == false);
	bIsUpdatingWaterInfo = true;
	ON_SCOPE_EXIT {bIsUpdatingWaterInfo = false;};

	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();
	FDeferredUpdateResource::UpdateResources(RHICmdList);

	FRenderTarget* RenderTarget = Params.RenderTarget;
	FTexture* OutputTexture = Params.OutputTexture;

	TRefCountPtr<IPooledRenderTarget> ExtractedDepthTexture;
	TRefCountPtr<IPooledRenderTarget> ExtractedColorTexture;


	// Depth-only pass for actors which are considered the ground for water rendering
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::DepthPass);

		FSceneRenderer* DepthRenderer = Params.DepthRenderer;
		FSceneInterface* Scene = DepthRenderer->ViewFamily.Scene;

		DepthRenderer->RenderThreadBegin(RHICmdList);

		SCOPED_DRAW_EVENT(RHICmdList, DepthRendering_RT);

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("WaterInfoDepthRendering"), ERDGBuilderFlags::AllowParallelExecute);

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
		
		if (Scene->GetShadingPath() == EShadingPath::Mobile)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "CaptureSceneColor");
			CopySceneCaptureComponentToTarget(
				GraphBuilder,
				DepthTexture,
				DepthRenderer->ViewFamily,
				MakeStridedViewOfBase<const FSceneView>(MakeArrayView(DepthRenderer->Views)));
		}
		else
		{
			FRHICopyTextureInfo CopyInfo;
			AddCopyTexturePass(GraphBuilder, TargetTexture, DepthTexture, CopyInfo);
		}
		
		// We currently can't have multiple scene renderers run within the same RDGBuilder. Therefore, we must
		// extract the texture to allow it to survive until the water info pass which runs in a later RDG graph (however, still within the same frame).
		GraphBuilder.QueueTextureExtraction(DepthTexture, &ExtractedDepthTexture, ERDGResourceExtractionFlags::AllowTransient);

		GraphBuilder.Execute();

		DepthRenderer->RenderThreadEnd(RHICmdList);
	}

	// Render the water bodies' data including flow, zoffset, depth
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::ColorPass);

		FSceneRenderer* ColorRenderer = Params.ColorRenderer;
		FSceneInterface* Scene = ColorRenderer->ViewFamily.Scene;

		FEnableWaterInfoMeshesScoped ScopedWaterInfoMeshEnable(ColorRenderer->Scene, ColorRenderer->Views[0]);

		ColorRenderer->RenderThreadBegin(RHICmdList);

		FViewInfo& View = ColorRenderer->Views[0];

		SCOPED_DRAW_EVENT(RHICmdList, ColorRendering_RT);

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("WaterInfoColorRendering"), ERDGBuilderFlags::AllowParallelExecute);

		FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, RenderTarget->GetRenderTargetTexture(), TEXT("WaterColorTarget"));

		FRDGTextureDesc ColorTextureDesc(TargetTexture->Desc);
		ColorTextureDesc.Format = PF_A32B32G32R32F;
		FRDGTextureRef ColorTexture = GraphBuilder.CreateTexture(ColorTextureDesc, TEXT("WaterColorTexture"));

		AddClearRenderTargetPass(GraphBuilder, TargetTexture, FLinearColor::Black, View.UnscaledViewRect);

		View.bDisableQuerySubmissions = true;
		View.bIgnoreExistingQueries = true;

		{
			RDG_RHI_EVENT_SCOPE(GraphBuilder, RenderWaterInfoColor);
			ColorRenderer->Render(GraphBuilder);
		}

		// This CopySceneCaptureComponentToTarget is required on all platforms as it extracts a higher pixel depth texture than the scene render target so we can't just copy out of it
		{
			RDG_EVENT_SCOPE(GraphBuilder, "CaptureSceneColor");
			CopySceneCaptureComponentToTarget(
				GraphBuilder,
				ColorTexture,
				ColorRenderer->ViewFamily,
				MakeStridedViewOfBase<const FSceneView>(MakeArrayView(ColorRenderer->Views)));
		}

		// We currently can't have multiple scene renderers run within the same RDGBuilder. Therefore, we must
		// extract the texture to allow it to survive until the water info pass which runs in a later RDG graph (however, still within the same frame).
		GraphBuilder.QueueTextureExtraction(ColorTexture, &ExtractedColorTexture, ERDGResourceExtractionFlags::AllowTransient);

		GraphBuilder.Execute();
		
		ColorRenderer->RenderThreadEnd(RHICmdList);
	}

	// Depth-only pass for water body dilated sections which get composited back into the water info texture at a lower priority than regular water body data
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::DilationPass);

		FSceneRenderer* DilationRenderer = Params.DilationRenderer;
		FSceneInterface* Scene = DilationRenderer->ViewFamily.Scene;

		FEnableWaterInfoMeshesScoped ScopedWaterInfoMeshEnable(DilationRenderer->Scene, DilationRenderer->Views[0]);

		DilationRenderer->RenderThreadBegin(RHICmdList);

		FViewInfo& View = DilationRenderer->Views[0];

		SCOPED_DRAW_EVENT(RHICmdList, DilationRendering_RT);

		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("WaterInfoDilationRendering"), ERDGBuilderFlags::AllowParallelExecute);

		FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, RenderTarget->GetRenderTargetTexture(), TEXT("WaterDilationTarget"));

		FRDGTextureRef DepthTexture = GraphBuilder.RegisterExternalTexture(ExtractedDepthTexture);
		FRDGTextureRef ColorTexture = GraphBuilder.RegisterExternalTexture(ExtractedColorTexture);

		AddClearRenderTargetPass(GraphBuilder, TargetTexture, FLinearColor::Black, View.UnscaledViewRect);

		View.bDisableQuerySubmissions = true;
		View.bIgnoreExistingQueries = true;

		{
			RDG_RHI_EVENT_SCOPE(GraphBuilder, RenderWaterInfoDilation);
			DilationRenderer->Render(GraphBuilder);
		}

		FRDGTextureDesc TextureDesc(TargetTexture->Desc);
		FRDGTextureRef DilationTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("WaterDilationTexture"));

		if (Scene->GetShadingPath() == EShadingPath::Mobile)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "CaptureSceneColor");
			CopySceneCaptureComponentToTarget(
				GraphBuilder,
				DilationTexture,
				DilationRenderer->ViewFamily,
				MakeStridedViewOfBase<const FSceneView>(MakeArrayView(DilationRenderer->Views)));
		}
		else
		{
			FRHICopyTextureInfo CopyInfo;
			AddCopyTexturePass(GraphBuilder, TargetTexture, DilationTexture, CopyInfo);
		}

		FRDGTextureRef MergeTargetTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("WaterInfoMerged"));
		MergeWaterInfoAndDepth(GraphBuilder, DilationRenderer->ViewFamily, DilationRenderer->Views[0], MergeTargetTexture, DepthTexture, ColorTexture, DilationTexture, Params);

		FRDGTextureRef FinalizedTexture = GraphBuilder.CreateTexture(TargetTexture->Desc, TEXT("WaterInfoFinalized"));
		FinalizeWaterInfo(GraphBuilder, DilationRenderer->ViewFamily, DilationRenderer->Views[0], MergeTargetTexture, FinalizedTexture, Params);
		
		FRDGTextureRef ShaderResourceTexture = RegisterExternalTexture(GraphBuilder, OutputTexture->TextureRHI, TEXT("WaterInfoResolve"));
		AddCopyTexturePass(GraphBuilder, FinalizedTexture, ShaderResourceTexture);
		GraphBuilder.Execute();

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

	ShowFlags.SetDisableOcclusionQueries(true);
	ShowFlags.SetVirtualShadowMapPersistentData(false);
	
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
	// Must be set to false to prevent the renders from using different VSM page pool sizes leading to unnecessary reallocations.
	ViewInitOptions.bIsSceneCapture = false;

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

	ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(ViewFamily.Scene));
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

	static auto* CVarRenderCaptureNextWaterInfoDraws = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.WaterInfo.RenderCaptureNextWaterInfoDraws"));
	int32 RenderCaptureNextWaterInfoDraws = CVarRenderCaptureNextWaterInfoDraws ? CVarRenderCaptureNextWaterInfoDraws->AsVariableInt()->GetValueOnGameThread() : 0;
	RenderCaptureInterface::FScopedCapture RenderCapture((RenderCaptureNextWaterInfoDraws != 0), TEXT("RenderWaterInfo"));
	if (RenderCaptureNextWaterInfoDraws != 0)
	{
		RenderCaptureNextWaterInfoDraws = FMath::Max(0, RenderCaptureNextWaterInfoDraws - 1);
		CVarRenderCaptureNextWaterInfoDraws->Set(RenderCaptureNextWaterInfoDraws);
	}

	if (!IsValid(Context.TextureRenderTarget) || Scene == nullptr)
	{
		return;
	}
	const FVector ZoneExtent = Context.ZoneToRender->GetDynamicWaterInfoExtent();

	FVector ViewLocation = Context.ZoneToRender->GetDynamicWaterInfoCenter();
	ViewLocation.Z = Context.CaptureZ;

	const FBox2D CaptureBounds(FVector2D(ViewLocation - ZoneExtent), FVector2D(ViewLocation + ZoneExtent));

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
	if (Context.GroundPrimitiveComponents.Num() > 0)
	{
		ComponentsToRenderInDepthPass.Reserve(Context.GroundPrimitiveComponents.Num());
		for (TWeakObjectPtr<UPrimitiveComponent> GroundPrimComp : Context.GroundPrimitiveComponents)
		{
			if (GroundPrimComp.IsValid())
			{
				ComponentsToRenderInDepthPass.Add(GroundPrimComp.Get()->GetPrimitiveSceneId());
			}
		}
	}
	CreateSceneRendererParams.CaptureSource = SCS_DeviceDepth;
	CreateSceneRendererParams.ShowOnlyPrimitives = MoveTemp(ComponentsToRenderInDepthPass);
	FSceneRenderer* DepthRenderer = CreateWaterInfoSceneRenderer(CreateSceneRendererParams);
	
	TSet<FPrimitiveComponentId> ComponentsToRenderInColorPass;
	TSet<FPrimitiveComponentId> ComponentsToRenderInDilationPass;
	if (Context.WaterBodies.Num() > 0)
	{
		ComponentsToRenderInColorPass.Reserve(Context.WaterBodies.Num());
		ComponentsToRenderInDilationPass.Reserve(Context.WaterBodies.Num());
		for (const UWaterBodyComponent* WaterBodyToRender : Context.WaterBodies)
		{
			if (!IsValid(WaterBodyToRender))
			{
				continue;
			}

			// Perform our own simple culling based on the known Capture bounds:
			const FBox WaterBodyBounds = WaterBodyToRender->Bounds.GetBox();
			if (CaptureBounds.Intersect(FBox2D(FVector2D(WaterBodyBounds.Min), FVector2D(WaterBodyBounds.Max))))
			{
				ComponentsToRenderInColorPass.Add(WaterBodyToRender->GetWaterInfoMeshComponent()->GetPrimitiveSceneId());
				ComponentsToRenderInDilationPass.Add(WaterBodyToRender->GetDilatedWaterInfoMeshComponent()->GetPrimitiveSceneId());
			}
		}
	}
	CreateSceneRendererParams.CaptureSource = SCS_SceneColorSceneDepth;
	CreateSceneRendererParams.ShowOnlyPrimitives = MoveTemp(ComponentsToRenderInColorPass);
	FSceneRenderer* ColorRenderer = CreateWaterInfoSceneRenderer(CreateSceneRendererParams);

	CreateSceneRendererParams.CaptureSource = SCS_DeviceDepth;
	CreateSceneRendererParams.ShowOnlyPrimitives = MoveTemp(ComponentsToRenderInDilationPass);
	FSceneRenderer* DilationRenderer = CreateWaterInfoSceneRenderer(CreateSceneRendererParams);

	FTextureRenderTargetResource* TextureRenderTargetResource = Context.TextureRenderTarget->GameThread_GetRenderTargetResource();

	FUpdateWaterInfoParams Params;
	Params.Scene = Scene;
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

	UE::RenderCommandPipe::FSyncScope SyncScope;

	ENQUEUE_RENDER_COMMAND(WaterInfoCommand)(
	[Params, ZoneName = Context.ZoneToRender->GetActorNameOrLabel()](FRHICommandListImmediate& RHICmdList)
		{
			SCOPED_DRAW_EVENTF(RHICmdList, WaterZoneInfoRendering_RT, TEXT("RenderWaterInfo_%s"), *ZoneName);

			UpdateWaterInfoRendering_RenderThread(RHICmdList, Params);
		});
}

void UpdateWaterInfoRendering2(FSceneView& InView, const TMap<AWaterZone*, UE::WaterInfo::FRenderingContext>& WaterInfoContexts)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::UpdateWaterInfoRendering2);

	InView.WaterInfoTextureRenderingParams.Reset();
	for (const TPair<AWaterZone*, UE::WaterInfo::FRenderingContext>& Pair : WaterInfoContexts)
	{
		const UE::WaterInfo::FRenderingContext& Context(Pair.Value);

		if (!IsValid(Context.TextureRenderTarget))
		{
			continue;
		}
		const FVector ZoneExtent = Context.ZoneToRender->GetDynamicWaterInfoExtent();

		FVector ViewLocation = Context.ZoneToRender->GetDynamicWaterInfoCenter();
		ViewLocation.Z = Context.CaptureZ;

		const FBox2D CaptureBounds(FVector2D(ViewLocation - ZoneExtent), FVector2D(ViewLocation + ZoneExtent));

		// Zone rendering always happens facing towards negative z.
		const FVector LookAt = ViewLocation - FVector(0.f, 0.f, 1.f);

		FSceneView::FWaterInfoTextureRenderingParams RenderingParams;
		RenderingParams.RenderTarget = Context.TextureRenderTarget->GameThread_GetRenderTargetResource();
		RenderingParams.ViewLocation = ViewLocation;
		RenderingParams.ViewRotationMatrix = FLookAtMatrix(ViewLocation, LookAt, FVector(0.f, -1.f, 0.f));
		RenderingParams.ViewRotationMatrix = RenderingParams.ViewRotationMatrix.RemoveTranslation();
		RenderingParams.ViewRotationMatrix.RemoveScaling();
		RenderingParams.ProjectionMatrix = BuildOrthoMatrix(ZoneExtent.X, ZoneExtent.Y);
		RenderingParams.CaptureZ = ViewLocation.Z;
		RenderingParams.WaterHeightExtents = Context.ZoneToRender->GetWaterHeightExtents();
		RenderingParams.GroundZMin = Context.ZoneToRender->GetGroundZMin();
		RenderingParams.VelocityBlurRadius = Context.ZoneToRender->GetVelocityBlurRadius();
		RenderingParams.WaterZoneExtents = ZoneExtent;

		if (Context.GroundPrimitiveComponents.Num() > 0)
		{
			RenderingParams.TerrainComponentIds.Reserve(Context.GroundPrimitiveComponents.Num());
			for (TWeakObjectPtr<UPrimitiveComponent> GroundPrimComp : Context.GroundPrimitiveComponents)
			{
				if (GroundPrimComp.IsValid())
				{
					RenderingParams.TerrainComponentIds.Add(GroundPrimComp.Get()->GetPrimitiveSceneId());
				}
			}
		}
		if (Context.WaterBodies.Num() > 0)
		{
			RenderingParams.WaterBodyComponentIds.Reserve(Context.WaterBodies.Num());
			RenderingParams.DilatedWaterBodyComponentIds.Reserve(Context.WaterBodies.Num());
			for (const UWaterBodyComponent* WaterBodyToRender : Context.WaterBodies)
			{
				if (!IsValid(WaterBodyToRender))
				{
					continue;
				}

				// Perform our own simple culling based on the known Capture bounds:
				const FBox WaterBodyBounds = WaterBodyToRender->Bounds.GetBox();
				if (CaptureBounds.Intersect(FBox2D(FVector2D(WaterBodyBounds.Min), FVector2D(WaterBodyBounds.Max))))
				{
					RenderingParams.WaterBodyComponentIds.Add(WaterBodyToRender->GetWaterInfoMeshComponent()->GetPrimitiveSceneId());
					RenderingParams.DilatedWaterBodyComponentIds.Add(WaterBodyToRender->GetDilatedWaterInfoMeshComponent()->GetPrimitiveSceneId());
				}
			}
		}

		InView.WaterInfoTextureRenderingParams.Add(MoveTemp(RenderingParams));
	}
}


// ----------------------------------------------------------------------------------

/** Base class containing common functionalities for all water info passes. */
class FWaterInfoCustomRenderPassBase : public FCustomRenderPassBase
{
public:
	FWaterInfoCustomRenderPassBase(const FString& InDebugName, FCustomRenderPassBase::ERenderMode InRenderMode, FCustomRenderPassBase::ERenderOutput InRenderOutput, const FIntPoint& InRenderTargetSize)
		: FCustomRenderPassBase(InDebugName, InRenderMode, InRenderOutput, InRenderTargetSize)
	{
	}

	// Abstract class :
	virtual const FName& GetTypeName() const PURE_VIRTUAL(FWaterInfoCustomRenderPassBase::GetTypeName, static FName Name; return Name;);
};


// ----------------------------------------------------------------------------------

class FWaterInfoRenderingDepthPass final : public FWaterInfoCustomRenderPassBase
{
public:
	IMPLEMENT_CUSTOM_RENDER_PASS(FWaterInfoRenderingDepthPass);

	FWaterInfoRenderingDepthPass(const FIntPoint& InRenderTargetSize, const TMap<uint32, int32>& InLandscapeLODOverrides)
		: FWaterInfoCustomRenderPassBase(TEXT("WaterInfoDepthPass"), FCustomRenderPassBase::ERenderMode::DepthPass, FCustomRenderPassBase::ERenderOutput::DeviceDepth, InRenderTargetSize)
	{
		if (!InLandscapeLODOverrides.IsEmpty())
		{
			SetUserData(MakeUnique<FLandscapeLODOverridesCustomRenderPassUserData>(InLandscapeLODOverrides));
		}
	}

	virtual void OnPreRender(FRDGBuilder& GraphBuilder) override
	{
		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(RenderTargetSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource);
		RenderTargetTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("WaterDepthTexture"));
		AddClearRenderTargetPass(GraphBuilder, RenderTargetTexture, FLinearColor::Black, Views[0]->UnscaledViewRect);
	}
};

const FName& GetWaterInfoDepthPassName() { return FWaterInfoRenderingDepthPass::GetTypeNameStatic(); }


// ----------------------------------------------------------------------------------

class FWaterInfoRenderingColorPass final : public FWaterInfoCustomRenderPassBase
{
public:
	IMPLEMENT_CUSTOM_RENDER_PASS(FWaterInfoRenderingColorPass);

	FWaterInfoRenderingColorPass(const FIntPoint& InRenderTargetSize)
		: FWaterInfoCustomRenderPassBase(TEXT("WaterInfoColorPass"), FCustomRenderPassBase::ERenderMode::DepthAndBasePass, FCustomRenderPassBase::ERenderOutput::SceneColorAndDepth, InRenderTargetSize)
	{}

	virtual void OnPreRender(FRDGBuilder& GraphBuilder) override
	{
		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(RenderTargetSize, PF_A32B32G32R32F, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource);
		RenderTargetTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("WaterColorTexture"));
		AddClearRenderTargetPass(GraphBuilder, RenderTargetTexture, FLinearColor::Black, Views[0]->UnscaledViewRect);
	}
};

const FName& GetWaterInfoColorPassName() { return FWaterInfoRenderingColorPass::GetTypeNameStatic(); }


// ----------------------------------------------------------------------------------

class FWaterInfoRenderingDilationPass final : public FWaterInfoCustomRenderPassBase
{
public:
	IMPLEMENT_CUSTOM_RENDER_PASS(FWaterInfoRenderingDilationPass);

	FWaterInfoRenderingDilationPass(const FIntPoint& InRenderTargetSize)
		: FWaterInfoCustomRenderPassBase(TEXT("WaterInfoDilationPass"), FCustomRenderPassBase::ERenderMode::DepthPass, FCustomRenderPassBase::ERenderOutput::DeviceDepth, InRenderTargetSize)
	{}

	virtual void OnPreRender(FRDGBuilder& GraphBuilder) override
	{
		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(RenderTargetSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource);
		RenderTargetTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("WaterDilationTexture"));
		AddClearRenderTargetPass(GraphBuilder, RenderTargetTexture, FLinearColor::Black, Views[0]->UnscaledViewRect);
	}
	
	virtual void OnPostRender(FRDGBuilder& GraphBuilder) override
	{
		FRDGTextureDesc TextureDesc(RenderTargetTexture->Desc);		
		FRDGTextureRef MergeTargetTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("WaterInfoMerged"));
		MergeWaterInfoAndDepth(GraphBuilder, *Views[0]->Family, *Views[0], MergeTargetTexture, DepthPass->GetRenderTargetTexture(), ColorPass->GetRenderTargetTexture(), RenderTargetTexture, Params);
	
		FRDGTextureRef FinalizedTexture = GraphBuilder.CreateTexture(RenderTargetTexture->Desc, TEXT("WaterInfoFinalized"));
		FinalizeWaterInfo(GraphBuilder, *Views[0]->Family, *Views[0], MergeTargetTexture, FinalizedTexture, Params);

		FRDGTextureRef WaterInfoTexture = RegisterExternalTexture(GraphBuilder, WaterInfoRenderTarget->GetRenderTargetTexture(), TEXT("WaterInfoTexture"));
		AddCopyTexturePass(GraphBuilder, FinalizedTexture, WaterInfoTexture);
		GraphBuilder.UseExternalAccessMode(WaterInfoTexture, ERHIAccess::SRVMask);
	}

	FRenderTarget* WaterInfoRenderTarget = nullptr;
	FWaterInfoRenderingDepthPass* DepthPass = nullptr;
	FWaterInfoRenderingColorPass* ColorPass = nullptr;
	FUpdateWaterInfoParams Params;
};

const FName& GetWaterInfoDilationPassName() { return FWaterInfoRenderingDilationPass::GetTypeNameStatic(); }


// ----------------------------------------------------------------------------------

static TMap<uint32, int32> GatherLandscapeLODOverrides(const UWorld* World, const FIntPoint& RenderTargetSize, const FVector& WaterZoneExtents)
{
	// In order to prevent overdrawing the landscape components, we compute the lowest-detailed LOD level which satisfies the pixel coverage of the Water Info texture
	// and force it on all landscape components. This override is set different per Landscape actor in case there are multiple under the same water zone.
	//
	// Ex: If the WaterInfoTexture only has 1 pixel per 100 units, and the highest landscape LOD has 1 vertex per 20 units, we don't need to use the maximum landscape LOD
	// and can force a lower level of detail (in this case LOD2) while still satisfying the resolution of the water info texture.

	const double MinWaterInfoTextureExtent = FMath::Min(RenderTargetSize.X, RenderTargetSize.Y);
	const double MaxWaterZoneExtent = FMath::Max(WaterZoneExtents.X, WaterZoneExtents.Y);
	const double WaterInfoUnitsPerPixel =  MaxWaterZoneExtent / MinWaterInfoTextureExtent;

	TMap<uint32, int32> LandscapeLODOverrides;
	for (const ALandscapeProxy* LandscapeProxy : TActorRange<ALandscapeProxy>(World))
	{
		if (LandscapeProxy == nullptr)
		{
			continue;
		}
		
		const uint32 LandscapeKey = LandscapeProxy->ComputeLandscapeKey();
		int32 OptimalLODLevel = INDEX_NONE;

		// All components within the same landscape (and thus its render system) should have the same number of quads and the same extent.
		// therefore we can simply find the first component and compute its optimal LOD level.
		const double FullExtent = LandscapeProxy->SubsectionSizeQuads * FMath::Max(LandscapeProxy->GetTransform().GetScale3D().X, LandscapeProxy->GetTransform().GetScale3D().Y);
		const double NumQuads = LandscapeProxy->ComponentSizeQuads;
		const double LandscapeResolution = FullExtent / NumQuads;

		// Double the required landscape resolution to achieve 2 quads per pixel.
		const double LandscapeComponentUnitsPerQuad = 2.0 * LandscapeResolution;
		check(LandscapeComponentUnitsPerQuad > 0.f);

		// Derived from:
		// (ComponentWorldExtent / WaterInfoWorldspaceExtent) * WaterInfoTextureResolution = (NumComponentQuads / 2 ^ (LodLevel))
		OptimalLODLevel = FMath::Max(WaterInfoRenderLandscapeMinimumMipLevel, FMath::FloorToInt(FMath::Log2(WaterInfoUnitsPerPixel / LandscapeComponentUnitsPerQuad)));

		LandscapeLODOverrides.Add(LandscapeKey, OptimalLODLevel);
	}

	return LandscapeLODOverrides;
}

void UpdateWaterInfoRendering_CustomRenderPass(
	FSceneInterface* Scene,
	const FSceneViewFamily& ViewFamily,
	const WaterInfo::FRenderingContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WaterInfo::UpdateWaterInfoRendering_CustomRenderPass);

	if (!IsValid(Context.TextureRenderTarget) || Scene == nullptr)
	{
		return;
	}

	static auto* CVarRenderCaptureNextWaterInfoDraws = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.WaterInfo.RenderCaptureNextWaterInfoDraws"));
	int32 RenderCaptureNextWaterInfoDraws = CVarRenderCaptureNextWaterInfoDraws ? CVarRenderCaptureNextWaterInfoDraws->AsVariableInt()->GetValueOnGameThread() : 0;
	bool bPerformRenderCapture = false;
	if (RenderCaptureNextWaterInfoDraws != 0)
	{
		RenderCaptureNextWaterInfoDraws = FMath::Max(0, RenderCaptureNextWaterInfoDraws - 1);
		CVarRenderCaptureNextWaterInfoDraws->Set(RenderCaptureNextWaterInfoDraws);
		bPerformRenderCapture = true;
	}

	const FVector ZoneExtent = Context.ZoneToRender->GetDynamicWaterInfoExtent();

	FVector ViewLocation = Context.ZoneToRender->GetDynamicWaterInfoCenter();
	ViewLocation.Z = Context.CaptureZ;

	const FBox2D CaptureBounds(FVector2D(ViewLocation - ZoneExtent), FVector2D(ViewLocation + ZoneExtent));

	// Zone rendering always happens facing towards negative z.
	const FVector LookAt = ViewLocation - FVector(0.f, 0.f, 1.f);

	const FIntPoint RenderTargetSize(Context.TextureRenderTarget->GetSurfaceWidth(), Context.TextureRenderTarget->GetSurfaceHeight());

	FTextureRenderTargetResource* TextureRenderTargetResource = Context.TextureRenderTarget->GameThread_GetRenderTargetResource();

	FSceneInterface::FCustomRenderPassRendererInput PassInput;
	PassInput.ViewLocation = ViewLocation;
	PassInput.ViewRotationMatrix = FLookAtMatrix(ViewLocation, LookAt, FVector(0.f, -1.f, 0.f));
	PassInput.ViewRotationMatrix = PassInput.ViewRotationMatrix.RemoveTranslation();
	PassInput.ViewRotationMatrix.RemoveScaling();
	PassInput.ProjectionMatrix = BuildOrthoMatrix(ZoneExtent.X, ZoneExtent.Y);
	PassInput.ViewActor = Context.ZoneToRender;

	TSet<FPrimitiveComponentId> ComponentsToRenderInDepthPass;
	if (Context.GroundPrimitiveComponents.Num() > 0)
	{
		ComponentsToRenderInDepthPass.Reserve(Context.GroundPrimitiveComponents.Num());
		for (TWeakObjectPtr<UPrimitiveComponent> GroundPrimComp : Context.GroundPrimitiveComponents)
		{
			if (GroundPrimComp.IsValid())
			{
				ComponentsToRenderInDepthPass.Add(GroundPrimComp.Get()->GetPrimitiveSceneId());
			}
		}
	}
	PassInput.ShowOnlyPrimitives = MoveTemp(ComponentsToRenderInDepthPass);

	FWaterInfoRenderingDepthPass* DepthPass = new FWaterInfoRenderingDepthPass(RenderTargetSize, GatherLandscapeLODOverrides(Scene->GetWorld(), RenderTargetSize, ZoneExtent));
	if (bPerformRenderCapture)
	{
		// Initiate a render capture when this pass runs :
		DepthPass->PerformRenderCapture(FCustomRenderPassBase::ERenderCaptureType::BeginCapture);
	}
	PassInput.CustomRenderPass = DepthPass;
	Scene->AddCustomRenderPass(&ViewFamily, PassInput);

	TSet<FPrimitiveComponentId> ComponentsToRenderInColorPass;
	TSet<FPrimitiveComponentId> ComponentsToRenderInDilationPass;
	if (Context.WaterBodies.Num() > 0)
	{
		ComponentsToRenderInColorPass.Reserve(Context.WaterBodies.Num());
		ComponentsToRenderInDilationPass.Reserve(Context.WaterBodies.Num());
		for (const UWaterBodyComponent* WaterBodyToRender : Context.WaterBodies)
		{
			if (!IsValid(WaterBodyToRender))
			{
				continue;
			}

			// Perform our own simple culling based on the known Capture bounds:
			const FBox WaterBodyBounds = WaterBodyToRender->Bounds.GetBox();
			if (CaptureBounds.Intersect(FBox2D(FVector2D(WaterBodyBounds.Min), FVector2D(WaterBodyBounds.Max))))
			{
				ComponentsToRenderInColorPass.Add(WaterBodyToRender->GetWaterInfoMeshComponent()->GetPrimitiveSceneId());
				ComponentsToRenderInDilationPass.Add(WaterBodyToRender->GetDilatedWaterInfoMeshComponent()->GetPrimitiveSceneId());
			}
		}
	}
	PassInput.ShowOnlyPrimitives = MoveTemp(ComponentsToRenderInColorPass);
	FWaterInfoRenderingColorPass* ColorPass = new FWaterInfoRenderingColorPass(RenderTargetSize);
	PassInput.CustomRenderPass = ColorPass;
	Scene->AddCustomRenderPass(&ViewFamily, PassInput);

	FUpdateWaterInfoParams Params;
	Params.CaptureZ = ViewLocation.Z;
	Params.WaterHeightExtents = Context.ZoneToRender->GetWaterHeightExtents();
	Params.GroundZMin = Context.ZoneToRender->GetGroundZMin();
	Params.VelocityBlurRadius = Context.ZoneToRender->GetVelocityBlurRadius();
	Params.WaterZoneExtents = ZoneExtent;

	PassInput.ShowOnlyPrimitives = MoveTemp(ComponentsToRenderInDilationPass);
	FWaterInfoRenderingDilationPass* DilationPass = new FWaterInfoRenderingDilationPass(RenderTargetSize);
	DilationPass->DepthPass = DepthPass;
	DilationPass->ColorPass = ColorPass;
	DilationPass->WaterInfoRenderTarget = Context.TextureRenderTarget->GameThread_GetRenderTargetResource();
	DilationPass->Params = Params;
	if (bPerformRenderCapture)
	{
		// End a render capture when this pass runs :
		DilationPass->PerformRenderCapture(FCustomRenderPassBase::ERenderCaptureType::EndCapture);
	}
	PassInput.CustomRenderPass = DilationPass;
	Scene->AddCustomRenderPass(&ViewFamily, PassInput);
}

} // namespace WaterInfo
