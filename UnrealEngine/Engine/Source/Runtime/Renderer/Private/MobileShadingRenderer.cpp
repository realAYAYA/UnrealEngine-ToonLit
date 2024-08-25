// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileShadingRenderer.cpp: Scene rendering code for ES3/3.1 feature level.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "EngineGlobals.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "SceneUtils.h"
#include "UniformBuffer.h"
#include "Engine/BlendableInterface.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "FXSystem.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMobile.h"
#include "PostProcess/PostProcessUpscale.h"
#include "PostProcess/PostProcessCompositeEditorPrimitives.h"
#include "PostProcess/PostProcessHMD.h"
#include "PostProcess/PostProcessPixelProjectedReflectionMobile.h"
#include "PostProcess/PostProcessAmbientOcclusionMobile.h"
#include "PostProcess/PostProcessCombineLUTs.h"
#include "PostProcess/PostProcessTonemap.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "SceneViewExtension.h"
#include "ScreenRendering.h"
#include "ShaderPrint.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "MobileSeparateTranslucencyPass.h"
#include "MobileDistortionPass.h"
#include "VisualizeTexturePresent.h"
#include "RendererModule.h"
#include "EngineModule.h"
#include "GPUScene.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialSceneTextureId.h"
#include "SkyAtmosphereRendering.h"
#include "VisualizeTexture.h"
#include "VT/VirtualTextureFeedback.h"
#include "VT/VirtualTextureSystem.h"
#include "GPUSortManager.h"
#include "MobileBasePassRendering.h"
#include "MobileDeferredShadingPass.h"
#include "PlanarReflectionSceneProxy.h"
#include "InstanceCulling/InstanceCullingManager.h"
#include "InstanceCulling/InstanceCullingOcclusionQuery.h"
#include "SceneOcclusion.h"
#include "VariableRateShadingImageManager.h"
#include "SceneTextureReductions.h"
#include "GPUMessaging.h"
#include "Substrate/Substrate.h"
#include "RenderCore.h"
#include "RectLightTextureManager.h"
#include "IESTextureManager.h"
#include "SceneUniformBuffer.h"
#include "Engine/SpecularProfile.h"
#include "LocalFogVolumeRendering.h"
#include "SceneCaptureRendering.h"
#include "WaterInfoTextureRendering.h"
#include "Rendering/CustomRenderPass.h"

uint32 GetShadowQuality();

static TAutoConsoleVariable<int32> CVarMobileForceDepthResolve(
	TEXT("r.Mobile.ForceDepthResolve"),
	0,
	TEXT("0: Depth buffer is resolved by switching out render targets. (Default)\n")
	TEXT("1: Depth buffer is resolved by switching out render targets and drawing with the depth texture.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileAdrenoOcclusionMode(
	TEXT("r.Mobile.AdrenoOcclusionMode"),
	0,
	TEXT("0: Render occlusion queries after the base pass (default).\n")
	TEXT("1: Render occlusion queries after translucency and a flush, which can help Adreno devices in GL mode."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileCustomDepthForTranslucency(
	TEXT("r.Mobile.CustomDepthForTranslucency"),
	1,
	TEXT(" Whether to render custom depth/stencil if any tranclucency in the scene uses it. \n")
	TEXT(" 0 = Off \n")
	TEXT(" 1 = On [default]"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileTonemapSubpass(
	TEXT("r.Mobile.TonemapSubpass"),
	0,
	TEXT(" Whether to enable mobile tonemap subpass \n")
	TEXT(" 0 = Off [default]\n")
	TEXT(" 1 = On"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static bool IsMobileTonemapSubpassEnabled(const FStaticShaderPlatform Platform)
{
	static auto* MobileTonemapSubpassPathCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.TonemapSubpass"));
	return (MobileTonemapSubpassPathCvar && (MobileTonemapSubpassPathCvar->GetValueOnAnyThread() == 1)) && IsMobileHDR() && !IsMobileDeferredShadingEnabled(Platform);
}

DECLARE_GPU_STAT_NAMED(MobileSceneRender, TEXT("Mobile Scene Render"));

DECLARE_CYCLE_STAT(TEXT("SceneStart"), STAT_CLMM_SceneStart, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("SceneEnd"), STAT_CLMM_SceneEnd, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("InitViews"), STAT_CLMM_InitViews, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterInitViews"), STAT_CLMM_AfterInitViews, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Opaque"), STAT_CLMM_Opaque, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Occlusion"), STAT_CLMM_Occlusion, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Post"), STAT_CLMM_Post, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Translucency"), STAT_CLMM_Translucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Shadows"), STAT_CLMM_Shadows, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("SceneSimulation"), STAT_CLMM_SceneSim, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("PrePass"), STAT_CLM_MobilePrePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Velocity"), STAT_CLMM_Velocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("TranslucentVelocity"), STAT_CLMM_TranslucentVelocity, STATGROUP_CommandListMarkers);

extern bool IsMobileEyeAdaptationEnabled(const FViewInfo& View);

struct FMobileCustomDepthStencilUsage
{
	bool bUsesCustomDepthStencil = false;
	// whether CustomStencil is sampled as a textures
	bool bSamplesCustomStencil = false;
};

static FMobileCustomDepthStencilUsage GetCustomDepthStencilUsage(const FViewInfo& View)
{
	FMobileCustomDepthStencilUsage CustomDepthStencilUsage;

	// Find out whether there are primitives will render in custom depth pass or just always render custom depth
	if ((View.bHasCustomDepthPrimitives || GetCustomDepthMode() == ECustomDepthMode::EnabledWithStencil))
	{
		// Find out whether CustomDepth/Stencil used in translucent materials
		if (CVarMobileCustomDepthForTranslucency.GetValueOnAnyThread() != 0)
		{
			CustomDepthStencilUsage.bUsesCustomDepthStencil = View.bUsesCustomDepth || View.bUsesCustomStencil;
			CustomDepthStencilUsage.bSamplesCustomStencil = View.bUsesCustomStencil;
		}

		if (!CustomDepthStencilUsage.bSamplesCustomStencil)
		{
			// Find out whether post-process materials use CustomDepth/Stencil lookups
			const FBlendableManager& BlendableManager = View.FinalPostProcessSettings.BlendableManager;
			FBlendableEntry* BlendableIt = nullptr;
			while (FPostProcessMaterialNode* DataPtr = BlendableManager.IterateBlendables<FPostProcessMaterialNode>(BlendableIt))
			{
				if (DataPtr->IsValid())
				{
					FMaterialRenderProxy* Proxy = DataPtr->GetMaterialInterface()->GetRenderProxy();
					check(Proxy);

					const FMaterial& Material = Proxy->GetIncompleteMaterialWithFallback(View.GetFeatureLevel());
					const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();
					bool bUsesCustomDepth = MaterialShaderMap->UsesSceneTexture(PPI_CustomDepth);
					bool bUsesCustomStencil = MaterialShaderMap->UsesSceneTexture(PPI_CustomStencil);
					if (Material.IsStencilTestEnabled() || bUsesCustomDepth || bUsesCustomStencil)
					{
						CustomDepthStencilUsage.bUsesCustomDepthStencil |= true;
					}

					if (bUsesCustomStencil)
					{
						CustomDepthStencilUsage.bSamplesCustomStencil |= true;
						break;
					}
				}
			}
		}
	}
	
	return CustomDepthStencilUsage;
}

static void RenderOpaqueFX(
	FRDGBuilder& GraphBuilder,
	TConstStridedView<FSceneView> Views,
	FSceneUniformBuffer &SceneUniformBuffer,
	FFXSystemInterface* FXSystem,
	TRDGUniformBufferRef<FMobileSceneTextureUniformParameters> MobileSceneTexturesUniformBuffer)
{
	// Notify the FX system that opaque primitives have been rendered and we now have a valid depth buffer.
	if (FXSystem && Views.Num() > 0)
	{
		FXSystem->PostRenderOpaque(GraphBuilder, Views, SceneUniformBuffer, true /*bAllowGPUParticleUpdate*/);

		if (FGPUSortManager* GPUSortManager = FXSystem->GetGPUSortManager())
		{
			GPUSortManager->OnPostRenderOpaque(GraphBuilder);
		}
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FMobileRenderPassParameters, RENDERER_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileBasePassUniformParameters, MobileBasePass)
	SHADER_PARAMETER_STRUCT_REF(FMobileReflectionCaptureShaderData, ReflectionCapture)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalFogVolumeInstances)
	RDG_BUFFER_ACCESS(LocalFogVolumeTileDrawIndirectBuffer, ERHIAccess::IndirectArgs)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<uint>, LocalFogVolumeTileDataTexture)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, LocalFogVolumeTileDataBuffer)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, HalfResLocalFogVolumeViewSRV)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, HalfResLocalFogVolumeDepthSRV)
	RDG_TEXTURE_ACCESS(ColorGradingLUT, ERHIAccess::SRVGraphics)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

static bool PostProcessUsesSceneDepth(const FViewInfo& View)
{
	// Find out whether post-process materials use CustomDepth/Stencil lookups
	const FBlendableManager& BlendableManager = View.FinalPostProcessSettings.BlendableManager;
	FBlendableEntry* BlendableIt = nullptr;

	while (FPostProcessMaterialNode* DataPtr = BlendableManager.IterateBlendables<FPostProcessMaterialNode>(BlendableIt))
	{
		if (DataPtr->IsValid())
		{
			FMaterialRenderProxy* Proxy = DataPtr->GetMaterialInterface()->GetRenderProxy();
			check(Proxy);

			const FMaterial& Material = Proxy->GetIncompleteMaterialWithFallback(View.GetFeatureLevel());
			const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();
			if (MaterialShaderMap->UsesSceneTexture(PPI_SceneDepth))
			{
				return true;
			}
		}
	}
	return false;
}

static void PollOcclusionQueriesPass(FRDGBuilder& GraphBuilder)
{
	AddPass(GraphBuilder, RDG_EVENT_NAME("PollOcclusionQueries"), [](FRHICommandListImmediate& RHICmdList)
	{
		RHICmdList.PollOcclusionQueries();
	});
}

struct FRenderViewContext
{
	FViewInfo* ViewInfo;
	int32 ViewIndex;
	bool bIsFirstView;
	bool bIsLastView;
};
using FRenderViewContextArray = TArray<FRenderViewContext, TInlineAllocator<2, SceneRenderingAllocator>>;

static void GetRenderViews(TArrayView<FViewInfo> InViews, FRenderViewContextArray& RenderViews)
{
	for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ViewIndex++)
	{
		FViewInfo& View = InViews[ViewIndex];
		if (View.ShouldRenderView())
		{
			FRenderViewContext RenderView;
			RenderView.ViewInfo = &View;
			RenderView.ViewIndex = ViewIndex;
			RenderView.bIsFirstView = (RenderViews.Num() == 0);
			RenderView.bIsLastView = false;

			RenderViews.Add(RenderView);
		}
	}

	if (RenderViews.Num())
	{
		RenderViews.Last().bIsLastView = true;
	}
}

FMobileSceneRenderer::FMobileSceneRenderer(const FSceneViewFamily* InViewFamily, FHitProxyConsumer* HitProxyConsumer)
	: FSceneRenderer(InViewFamily, HitProxyConsumer)
	, bGammaSpace(!IsMobileHDR())
	, bDeferredShading(IsMobileDeferredShadingEnabled(ShaderPlatform))
	, bRequiresDBufferDecals(bDeferredShading ? false : IsUsingDBuffers(ShaderPlatform))
	, bUseVirtualTexturing(UseVirtualTexturing(ShaderPlatform) && GetRendererOutput() == FSceneRenderer::ERendererOutput::FinalSceneColor)
{
	bRenderToSceneColor = false;
	bRequiresMultiPass = false;
	bKeepDepthContent = false;
	bModulatedShadowsInUse = false;
	bShouldRenderCustomDepth = false;
	bRequiresPixelProjectedPlanarRelfectionPass = false;
	bRequiresAmbientOcclusionPass = false;
	bRequiresShadowProjections = false;
	bIsFullDepthPrepassEnabled = Scene->EarlyZPassMode == DDM_AllOpaque;
	bIsMaskedOnlyDepthPrepassEnabled = Scene->EarlyZPassMode == DDM_MaskedOnly;
	bEnableClusteredLocalLights = MobileForwardEnableLocalLights(ShaderPlatform);
	bEnableClusteredReflections = MobileForwardEnableClusteredReflections(ShaderPlatform);
	
	StandardTranslucencyPass = ViewFamily.AllowTranslucencyAfterDOF() ? ETranslucencyPass::TPT_TranslucencyStandard : ETranslucencyPass::TPT_AllTranslucency;
	StandardTranslucencyMeshPass = TranslucencyPassToMeshPass(StandardTranslucencyPass);

	// Don't do occlusion queries when doing scene captures
	for (FViewInfo& View : Views)
	{
		if (View.bIsSceneCapture)
		{
			View.bDisableQuerySubmissions = true;
			View.bIgnoreExistingQueries = true;
		}
	}

	NumMSAASamples = GetDefaultMSAACount(ERHIFeatureLevel::ES3_1);
	// As of UE 5.4 only vulkan supports inline (single pass) tonemap
	bTonemapSubpass = IsMobileTonemapSubpassEnabled(ShaderPlatform) && ViewFamily.bResolveScene && GetRendererOutput() == FSceneRenderer::ERendererOutput::FinalSceneColor;
	bTonemapSubpassInline = bTonemapSubpass && IsVulkanPlatform(ShaderPlatform) && (GRHISupportsMSAAShaderResolve || NumMSAASamples == 1);
	bRequiresSceneDepthAux = MobileRequiresSceneDepthAux(ShaderPlatform) && !bTonemapSubpass;
}

class FMobileDirLightShaderParamsRenderResource : public FRenderResource
{
public:
	using MobileDirLightUniformBufferRef = TUniformBufferRef<FMobileDirectionalLightShaderParameters>;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		UniformBufferRHI =
			MobileDirLightUniformBufferRef::CreateUniformBufferImmediate(
				FMobileDirectionalLightShaderParameters(),
				UniformBuffer_MultiFrame);
	}

	virtual void ReleaseRHI() override
	{
		UniformBufferRHI.SafeRelease();
	}

	MobileDirLightUniformBufferRef UniformBufferRHI;
};

TUniformBufferRef<FMobileDirectionalLightShaderParameters>& GetNullMobileDirectionalLightShaderParameters()
{
	static TGlobalResource<FMobileDirLightShaderParamsRenderResource>* NullLightParams;
	if (!NullLightParams)
	{
		NullLightParams = new TGlobalResource<FMobileDirLightShaderParamsRenderResource>();
	}
	check(!!NullLightParams->UniformBufferRHI);
	return NullLightParams->UniformBufferRHI;
}

void FMobileSceneRenderer::PrepareViewVisibilityLists()
{
	// Prepare view's visibility lists.
	// TODO: only do this when CSM + static is required.
	for (auto& View : Views)
	{
		FMobileCSMVisibilityInfo& MobileCSMVisibilityInfo = View.MobileCSMVisibilityInfo;
		// Init list of primitives that can receive Dynamic CSM.
		MobileCSMVisibilityInfo.MobilePrimitiveCSMReceiverVisibilityMap.Init(false, View.PrimitiveVisibilityMap.Num());

		// Init static mesh visibility info for CSM drawlist
		MobileCSMVisibilityInfo.MobileCSMStaticMeshVisibilityMap.Init(false, View.StaticMeshVisibilityMap.Num());

		// Init static mesh visibility info for default drawlist that excludes meshes in CSM only drawlist.
		MobileCSMVisibilityInfo.MobileNonCSMStaticMeshVisibilityMap = View.StaticMeshVisibilityMap;
	}
}

void FMobileSceneRenderer::SetupMobileBasePassAfterShadowInit(FExclusiveDepthStencil::Type BasePassDepthStencilAccess, TArrayView<FViewCommands> ViewCommandsPerView, FInstanceCullingManager& InstanceCullingManager)
{
	// Sort front to back on all platforms, even HSR benefits from it
	//const bool bWantsFrontToBackSorting = (GHardwareHiddenSurfaceRemoval == false);

	// compute keys for front to back sorting and dispatch pass setup.
	for (int32 ViewIndex = 0; ViewIndex < AllViews.Num(); ++ViewIndex)
	{
		FViewInfo& View = *AllViews[ViewIndex];
		FViewCommands& ViewCommands = ViewCommandsPerView[ViewIndex];

		FMeshPassProcessor* MeshPassProcessor = FPassProcessorManager::CreateMeshPassProcessor(EShadingPath::Mobile, EMeshPass::BasePass, Scene->GetFeatureLevel(), Scene, &View, nullptr);

		FMeshPassProcessor* BasePassCSMMeshPassProcessor = FPassProcessorManager::CreateMeshPassProcessor(EShadingPath::Mobile, EMeshPass::MobileBasePassCSM, Scene->GetFeatureLevel(), Scene, &View, nullptr);

		TArray<int32, TInlineAllocator<2> > ViewIds;
		ViewIds.Add(View.GPUSceneViewId);
		// Only apply instancing for ISR to main view passes
		EInstanceCullingMode InstanceCullingMode = View.IsInstancedStereoPass() ? EInstanceCullingMode::Stereo : EInstanceCullingMode::Normal;
		if (InstanceCullingMode == EInstanceCullingMode::Stereo)
		{
			check(View.GetInstancedView() != nullptr);
			ViewIds.Add(View.GetInstancedView()->GPUSceneViewId);
		}

		// Run sorting on BasePass, as it's ignored inside FSceneRenderer::SetupMeshPass, so it can be done after shadow init on mobile.
		FParallelMeshDrawCommandPass& Pass = View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass];
		if (ShouldDumpMeshDrawCommandInstancingStats())
		{
			Pass.SetDumpInstancingStats(GetMeshPassName(EMeshPass::BasePass));
		}

		FName PassName(GetMeshPassName(EMeshPass::BasePass));
		Pass.DispatchPassSetup(
			Scene,
			View,
			FInstanceCullingContext(PassName, ShaderPlatform, &InstanceCullingManager, ViewIds, nullptr, InstanceCullingMode),
			EMeshPass::BasePass,
			BasePassDepthStencilAccess,
			MeshPassProcessor,
			View.DynamicMeshElements,
			&View.DynamicMeshElementsPassRelevance,
			View.NumVisibleDynamicMeshElements[EMeshPass::BasePass],
			ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass],
			ViewCommands.DynamicMeshCommandBuildFlags[EMeshPass::BasePass],
			ViewCommands.NumDynamicMeshCommandBuildRequestElements[EMeshPass::BasePass],
			ViewCommands.MeshCommands[EMeshPass::BasePass],
			BasePassCSMMeshPassProcessor,
			&ViewCommands.MeshCommands[EMeshPass::MobileBasePassCSM]);
	}
}

/**
 * Initialize scene's views.
 * Check visibility, sort translucent items, etc.
 */
void FMobileSceneRenderer::InitViews(
	FRDGBuilder& GraphBuilder,
	FSceneTexturesConfig& SceneTexturesConfig,
	FInstanceCullingManager& InstanceCullingManager,
	FVirtualTextureUpdater* VirtualTextureUpdater,
	FInitViewTaskDatas& TaskDatas)
{
	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;
	
	SCOPED_DRAW_EVENT(RHICmdList, InitViews);

	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, InitViews_Scene);

	check(Scene);

	const bool bRendererOutputFinalSceneColor = (GetRendererOutput() == ERendererOutput::FinalSceneColor);

	PreVisibilityFrameSetup(GraphBuilder);

	if (InstanceCullingManager.IsEnabled()
		&& Scene->InstanceCullingOcclusionQueryRenderer
		&& Scene->InstanceCullingOcclusionQueryRenderer->InstanceOcclusionQueryBuffer)
	{
		InstanceCullingManager.InstanceOcclusionQueryBuffer = GraphBuilder.RegisterExternalBuffer(Scene->InstanceCullingOcclusionQueryRenderer->InstanceOcclusionQueryBuffer);
		InstanceCullingManager.InstanceOcclusionQueryBufferFormat = Scene->InstanceCullingOcclusionQueryRenderer->InstanceOcclusionQueryBufferFormat;
	}

	// Create GPU-side representation of the view for instance culling.
	InstanceCullingManager.AllocateViews(Views.Num());
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		Views[ViewIndex].GPUSceneViewId = InstanceCullingManager.RegisterView(Views[ViewIndex]);
		// Set the stereo instance factor as GetStereoPassInstanceFactor can't be called from inside an RDG pass.
		uint32 InstanceFactor = Views[ViewIndex].GetStereoPassInstanceFactor();
		Views[ViewIndex].InstanceFactor = InstanceFactor > 0 ? InstanceFactor : 1;
	}

	FILCUpdatePrimTaskData* ILCTaskData = nullptr;

	const FExclusiveDepthStencil::Type BasePassDepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilWrite;

	if (FXSystem && FXSystem->RequiresEarlyViewUniformBuffer() && Views.IsValidIndex(0) && bRendererOutputFinalSceneColor)
	{
		// This is to init the ViewUniformBuffer before rendering for the Niagara compute shader.
		// This needs to run before ComputeViewVisibility() is called, but the views normally initialize the ViewUniformBuffer after that (at the end of this method).

		// during ISR, instanced view RHI resources need to be initialized first.
		if (FViewInfo* InstancedView = const_cast<FViewInfo*>(Views[0].GetInstancedView()))
		{
			InstancedView->InitRHIResources();
		}
		Views[0].InitRHIResources();
		FXSystem->PostInitViews(GraphBuilder, GetSceneViews(), !ViewFamily.EngineShowFlags.HitProxies);
	}

	TaskDatas.VisibilityTaskData->ProcessRenderThreadTasks();
	TaskDatas.VisibilityTaskData->FinishGatherDynamicMeshElements(BasePassDepthStencilAccess, InstanceCullingManager, VirtualTextureUpdater);

	if (ShouldRenderVolumetricFog() && bRendererOutputFinalSceneColor)
	{
		SetupVolumetricFog();
	}
	PostVisibilityFrameSetup(ILCTaskData);

	FIntPoint RenderTargetSize = ViewFamily.RenderTarget->GetSizeXY();
	EPixelFormat RenderTargetPixelFormat = PF_Unknown;
	if (ViewFamily.RenderTarget->GetRenderTargetTexture().IsValid())
	{
		RenderTargetSize = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeXY();
		RenderTargetPixelFormat = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetFormat();
	}
	const bool bRequiresUpscale = ((int32)RenderTargetSize.X > FamilySize.X || (int32)RenderTargetSize.Y > FamilySize.Y);
	// ES requires that the back buffer and depth match dimensions.
	// For the most part this is not the case when using scene captures. Thus scene captures always render to scene color target.
	const bool bShouldCompositeEditorPrimitives = FSceneRenderer::ShouldCompositeEditorPrimitives(Views[0]);
	const bool bStereoRenderingAndHMD = ViewFamily.EngineShowFlags.StereoRendering && ViewFamily.EngineShowFlags.HMDDistortion;
	bRenderToSceneColor = !bGammaSpace 
						|| bStereoRenderingAndHMD 
						|| bRequiresUpscale 
						|| bShouldCompositeEditorPrimitives 
						|| Views[0].bIsSceneCapture 
						|| Views[0].bIsReflectionCapture 
						// If the resolve texture is not the same as the MSAA texture, we need to render to scene color and copy to back buffer.
						|| (NumMSAASamples > 1 && !RHISupportsSeparateMSAAAndResolveTextures(ShaderPlatform))
						|| (NumMSAASamples > 1 && (RenderTargetPixelFormat != PF_Unknown && RenderTargetPixelFormat != SceneTexturesConfig.ColorFormat))
						|| bIsFullDepthPrepassEnabled;

	bool bSceneDepthCapture = (
		ViewFamily.SceneCaptureSource == SCS_SceneColorSceneDepth ||
		ViewFamily.SceneCaptureSource == SCS_SceneDepth ||
		ViewFamily.SceneCaptureSource == SCS_DeviceDepth);
	// Check if any of the custom render passes outputs depth texture, used to decide whether to enable bPreciseDepthAux.
	for (FViewInfo* View : AllViews)
	{
		if (View->CustomRenderPass)
		{
			ESceneCaptureSource CaptureSource = View->CustomRenderPass->GetSceneCaptureSource();
			if (CaptureSource == SCS_SceneColorSceneDepth ||
				CaptureSource == SCS_SceneDepth ||
				CaptureSource == SCS_DeviceDepth)
			{
				bSceneDepthCapture = true;
				break;
			}
		}
	}

	const FPlanarReflectionSceneProxy* PlanarReflectionSceneProxy = Scene ? Scene->GetForwardPassGlobalPlanarReflection() : nullptr;

	bRequiresPixelProjectedPlanarRelfectionPass = IsUsingMobilePixelProjectedReflection(ShaderPlatform)
		&& PlanarReflectionSceneProxy != nullptr
		&& PlanarReflectionSceneProxy->RenderTarget != nullptr
		&& !Views[0].bIsReflectionCapture
		&& !ViewFamily.EngineShowFlags.HitProxies
		&& ViewFamily.EngineShowFlags.Lighting
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling
		&& !ViewFamily.UseDebugViewPS()
		&& bRendererOutputFinalSceneColor;

	bRequiresAmbientOcclusionPass = IsUsingMobileAmbientOcclusion(ShaderPlatform)
		&& Views[0].FinalPostProcessSettings.AmbientOcclusionIntensity > 0
		&& (Views[0].FinalPostProcessSettings.AmbientOcclusionStaticFraction >= 1 / 100.0f || (Scene && Scene->SkyLight && Scene->SkyLight->ProcessedTexture && Views[0].Family->EngineShowFlags.SkyLighting))
		&& ViewFamily.EngineShowFlags.Lighting
		&& !Views[0].bIsReflectionCapture
		&& !Views[0].bIsPlanarReflection
		&& !ViewFamily.EngineShowFlags.HitProxies
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling
		&& !ViewFamily.UseDebugViewPS()
		&& bRendererOutputFinalSceneColor;

	bShouldRenderVelocities = ShouldRenderVelocities();

	bRequiresShadowProjections = MobileUsesShadowMaskTexture(ShaderPlatform)
		&& ViewFamily.EngineShowFlags.Lighting
		&& !Views[0].bIsReflectionCapture
		&& !Views[0].bIsPlanarReflection
		&& !ViewFamily.EngineShowFlags.HitProxies
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling
		&& !ViewFamily.UseDebugViewPS()
		&& bRendererOutputFinalSceneColor;

	bShouldRenderHZB = ShouldRenderHZB() && bRendererOutputFinalSceneColor;

	// Whether we need to store depth for post-processing
	// On PowerVR we see flickering of shadows and depths not updating correctly if targets are discarded.
	const bool bForceDepthResolve = (CVarMobileForceDepthResolve.GetValueOnRenderThread() == 1);
	const bool bSeparateTranslucencyActive = IsMobileSeparateTranslucencyActive(Views.GetData(), Views.Num()); 
	const bool bPostProcessUsesSceneDepth = PostProcessUsesSceneDepth(Views[0]) || IsMobileDistortionActive(Views[0]);
	const bool bRequireSeparateViewPass = Views.Num() > 1 && !Views[0].bIsMobileMultiViewEnabled;
	bRequiresMultiPass = RequiresMultiPass(NumMSAASamples, ShaderPlatform);

	bKeepDepthContent =
		bRequiresMultiPass ||
		bForceDepthResolve ||
		bRequiresPixelProjectedPlanarRelfectionPass ||
		bSeparateTranslucencyActive ||
		Views[0].bIsReflectionCapture ||
		(bDeferredShading && bPostProcessUsesSceneDepth) ||
		(bDeferredShading && bSceneDepthCapture) ||
		Views[0].AntiAliasingMethod == AAM_TemporalAA ||
		bRequireSeparateViewPass ||
		bIsFullDepthPrepassEnabled ||
		GraphBuilder.IsDumpingFrame();
	// never keep MSAA depth if SceneDepthAux is enabled
	bKeepDepthContent = ((NumMSAASamples > 1) && bRequiresSceneDepthAux) ? false : bKeepDepthContent;

	// Depth is needed for Editor Primitives
	if (bShouldCompositeEditorPrimitives)
	{
		bKeepDepthContent = true;
	}

	// In the editor RHIs may split a render-pass into several cmd buffer submissions, so all targets need to Store
	if (IsSimulatedPlatform(ShaderPlatform))
	{
		bKeepDepthContent = true;
	}

	// Update the bKeepDepthContent based on the mobile renderer status.
	SceneTexturesConfig.bKeepDepthContent = bKeepDepthContent;
	// If we render in a single pass MSAA targets can be memoryless
    SceneTexturesConfig.bMemorylessMSAA = !(bRequiresMultiPass || bShouldCompositeEditorPrimitives || bRequireSeparateViewPass);
    SceneTexturesConfig.NumSamples = NumMSAASamples;
	SceneTexturesConfig.ExtraSceneColorCreateFlags |= (bTonemapSubpassInline ? TexCreate_InputAttachmentRead : TexCreate_None);
    SceneTexturesConfig.BuildSceneColorAndDepthFlags();
	if (bDeferredShading) 
	{
		SceneTexturesConfig.SetupMobileGBufferFlags(bRequiresMultiPass || GraphBuilder.IsDumpingFrame() || bRequireSeparateViewPass);
	}

	// Update the pixel projected reflection extent according to the settings in the PlanarReflectionComponent.
	if (bRequiresPixelProjectedPlanarRelfectionPass)
	{
		SceneTexturesConfig.MobilePixelProjectedReflectionExtent = PlanarReflectionSceneProxy->RenderTarget->GetSizeXY();
	}
	else
	{
		SceneTexturesConfig.MobilePixelProjectedReflectionExtent = FIntPoint::ZeroValue;
	}

	SceneTexturesConfig.bRequiresDepthAux = bRequiresSceneDepthAux;
	// When we capturing scene depth, use a more precise format for SceneDepthAux as it will be used as a source DepthTexture
	if (bSceneDepthCapture)
	{
		SceneTexturesConfig.bPreciseDepthAux = true;
	}
	
	// Find out whether custom depth pass should be rendered.
	{
		bool bCouldUseCustomDepthStencil = (!Scene->World || (Scene->World->WorldType != EWorldType::EditorPreview && Scene->World->WorldType != EWorldType::Inactive));
		FMobileCustomDepthStencilUsage CustomDepthStencilUsage;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			CustomDepthStencilUsage = GetCustomDepthStencilUsage(Views[ViewIndex]);
			Views[ViewIndex].bCustomDepthStencilValid = bCouldUseCustomDepthStencil && CustomDepthStencilUsage.bUsesCustomDepthStencil;
			bShouldRenderCustomDepth |= Views[ViewIndex].bCustomDepthStencilValid;
			SceneTexturesConfig.bSamplesCustomStencil |= bShouldRenderCustomDepth && CustomDepthStencilUsage.bSamplesCustomStencil;
		}
	}
	
	// Finalize and set the scene textures config.
	FSceneTexturesConfig::Set(SceneTexturesConfig);

	if (bRendererOutputFinalSceneColor)
	{
		// Initialise Sky/View resources before the view global uniform buffer is built.
		if (ShouldRenderSkyAtmosphere(Scene, ViewFamily.EngineShowFlags))
		{
			InitSkyAtmosphereForViews(RHICmdList);
		}

		if (bRequiresShadowProjections)
		{
			InitMobileShadowProjectionOutputs(RHICmdList, SceneTexturesConfig.Extent);
		}
		else
		{
			ReleaseMobileShadowProjectionOutputs();
		}
	}
		
	FRDGExternalAccessQueue ExternalAccessQueue;

	// initialize per-view uniform buffer.  Pass in shadow info as necessary.
	for (int32 ViewIndex = Views.Num() - 1; ViewIndex >= 0; --ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		
		View.UpdatePreExposure();

		// Initialize the view's RHI resources.
		View.InitRHIResources();
	}

	for (int32 i = 0; i < CustomRenderPassInfos.Num(); ++i)
	{
		for (FViewInfo& View : CustomRenderPassInfos[i].Views)
		{
			View.InitRHIResources();
		}		
	}

	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, UpdateGPUScene);

		for (int32 ViewIndex = 0; ViewIndex < AllViews.Num(); ViewIndex++)
		{
			FViewInfo& View = *AllViews[ViewIndex];
			Scene->GPUScene.UploadDynamicPrimitiveShaderDataForView(GraphBuilder, View);
			Scene->GPUScene.DebugRender(GraphBuilder, GetSceneUniforms(), View);
		}
	}

	if (bRendererOutputFinalSceneColor)
	{
		const bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows;
		if (bDynamicShadows)
		{
			// Setup dynamic shadows.
			TaskDatas.DynamicShadows = InitDynamicShadows(GraphBuilder, InstanceCullingManager);
		}
		else
		{
			// TODO: only do this when CSM + static is required.
			PrepareViewVisibilityLists();
		}
	}

	TaskDatas.VisibilityTaskData->Finish();

	if (bRendererOutputFinalSceneColor)
	{
		SetupMobileBasePassAfterShadowInit(BasePassDepthStencilAccess, TaskDatas.VisibilityTaskData->GetViewCommandsPerView(), InstanceCullingManager);

		// if we kicked off ILC update via task, wait and finalize.
		if (ILCTaskData)
		{
			Scene->IndirectLightingCache.FinalizeCacheUpdates(Scene, *this, *ILCTaskData);
		}
	}

	ExternalAccessQueue.Submit(GraphBuilder);

	InstanceCullingManager.BeginDeferredCulling(GraphBuilder, Scene->GPUScene);

	extern TSet<IPersistentViewUniformBufferExtension*> PersistentViewUniformBufferExtensions;

	for (IPersistentViewUniformBufferExtension* Extension : PersistentViewUniformBufferExtensions)
	{
		Extension->BeginFrame();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			// Must happen before RHI thread flush so any tasks we dispatch here can land in the idle gap during the flush
			Extension->PrepareView(&Views[ViewIndex]);
		}
	}

	if (bRendererOutputFinalSceneColor)
	{
		if (bDeferredShading ||
			bEnableClusteredLocalLights || 
			bEnableClusteredReflections)
		{
			SetupSceneReflectionCaptureBuffer(RHICmdList);
		}
		UpdateSkyReflectionUniformBuffer(RHICmdList);

		// Now that the indirect lighting cache is updated, we can update the uniform buffers.
		UpdatePrimitiveIndirectLightingCacheBuffers(RHICmdList);

		UpdateDirectionalLightUniformBuffers(GraphBuilder, Views[0]);
	}
}

static void BeginOcclusionScope(FRDGBuilder& GraphBuilder, TArray<FViewInfo>& Views)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		if (View.ShouldRenderView() && View.ViewState && View.ViewState->OcclusionFeedback.IsInitialized())
		{
			View.ViewState->OcclusionFeedback.BeginOcclusionScope(GraphBuilder);
		}
	}
}

static void EndOcclusionScope(FRDGBuilder& GraphBuilder, TArray<FViewInfo>& Views)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		if (View.ShouldRenderView() && View.ViewState && View.ViewState->OcclusionFeedback.IsInitialized())
		{
			View.ViewState->OcclusionFeedback.EndOcclusionScope(GraphBuilder);
		}
	}
}

/*
* Renders the Full Depth Prepass
*/
void FMobileSceneRenderer::RenderFullDepthPrepass(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> InViews, FSceneTextures& SceneTextures, bool bIsSceneCaptureRenderPass)
{
	FRenderTargetBindingSlots BasePassRenderTargets;
	BasePassRenderTargets.DepthStencil = FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	// If this is scene capture render pass, don't render occlusion
	BasePassRenderTargets.NumOcclusionQueries = bIsSceneCaptureRenderPass ? 0 : ComputeNumOcclusionQueriesToBatch();

	FRenderViewContextArray RenderViews;
	GetRenderViews(InViews, RenderViews);

	for (FRenderViewContext& ViewContext : RenderViews)
	{
		FViewInfo& View = *ViewContext.ViewInfo;

		if (!ViewContext.bIsFirstView)
		{
			BasePassRenderTargets.DepthStencil.SetDepthLoadAction(ERenderTargetLoadAction::ELoad);
			BasePassRenderTargets.DepthStencil.SetStencilLoadAction(ERenderTargetLoadAction::ELoad);
			BasePassRenderTargets.DepthStencil.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite);
		}

		View.BeginRenderView();

		auto* PassParameters = GraphBuilder.AllocParameters<FMobileRenderPassParameters>();
		PassParameters->RenderTargets = BasePassRenderTargets;
		PassParameters->View = View.GetShaderParameters();
		PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Opaque, EMobileSceneTextureSetupMode::None);
		
		View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

		// Render occlusion at the last view pass only, as they already loop through all views
		// If this is scene capture render pass, don't render occlusion.
		bool bDoOcclusionQueries = (ViewContext.bIsLastView && DoOcclusionQueries() && !bIsSceneCaptureRenderPass);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("FullDepthPrepass"),
			PassParameters,
			ERDGPassFlags::Raster,
			[this, PassParameters, &View, bDoOcclusionQueries](FRHICommandList& RHICmdList)
			{
				RenderPrePass(RHICmdList, View, &PassParameters->InstanceCullingDrawParams);

				if (bDoOcclusionQueries)
				{
					RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Occlusion));
					RenderOcclusion(RHICmdList);
				}
			});
	}

	FenceOcclusionTests(GraphBuilder);
}

void FMobileSceneRenderer::RenderMaskedPrePass(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	if (bIsMaskedOnlyDepthPrepassEnabled)
	{
		RenderPrePass(RHICmdList, View, &DepthPassInstanceCullingDrawParams);
	}
}

void FMobileSceneRenderer::RenderCustomRenderPassBasePass(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> InViews, FRDGTextureRef ViewFamilyTexture, FSceneTextures& SceneTextures)
{
	FRenderTargetBindingSlots BasePassRenderTargets;
	if (bDeferredShading)
	{
		FColorTargets ColorTargets = GetColorTargets_Deferred(SceneTextures);
		BasePassRenderTargets = InitRenderTargetBindings_Deferred(SceneTextures, ColorTargets);
	}
	else
	{
		BasePassRenderTargets = InitRenderTargetBindings_Forward(ViewFamilyTexture, SceneTextures);
	}

	FRenderViewContextArray RenderViews;
	GetRenderViews(InViews, RenderViews);

	for (FRenderViewContext& ViewContext : RenderViews)
	{
		FViewInfo& View = *ViewContext.ViewInfo;

		EMobileSceneTextureSetupMode SetupMode = bIsFullDepthPrepassEnabled ? EMobileSceneTextureSetupMode::SceneDepth : EMobileSceneTextureSetupMode::None;
		FMobileRenderPassParameters* PassParameters = GraphBuilder.AllocParameters<FMobileRenderPassParameters>();
		PassParameters->View = View.GetShaderParameters();
		PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Opaque, SetupMode);
		PassParameters->RenderTargets = BasePassRenderTargets;

		if (Scene->GPUScene.IsEnabled())
		{
			View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);
		}

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("RenderMobileBasePass"),
			PassParameters,
			ERDGPassFlags::Raster,
			[this, PassParameters, ViewContext, &SceneTextures](FRHICommandList& RHICmdList)
			{
				FViewInfo& View = *ViewContext.ViewInfo;
				RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Opaque));
				RenderMobileBasePass(RHICmdList, View, &PassParameters->InstanceCullingDrawParams);
			});

		if (!bIsFullDepthPrepassEnabled)
		{
			AddResolveSceneDepthPass(GraphBuilder, View, SceneTextures.Depth);
		}
		if (bRequiresSceneDepthAux)
		{
			AddResolveSceneColorPass(GraphBuilder, View, SceneTextures.DepthAux);
		}
	}
}

void FMobileSceneRenderer::Render(FRDGBuilder& GraphBuilder)
{
	if (!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}

	const ERendererOutput RendererOutput = GetRendererOutput();
	const bool bRendererOutputFinalSceneColor = (RendererOutput == ERendererOutput::FinalSceneColor);

	GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_SceneStart));

	RDG_RHI_EVENT_SCOPE(GraphBuilder, MobileSceneRender);
	RDG_RHI_GPU_STAT_SCOPE(GraphBuilder, MobileSceneRender);

	IVisibilityTaskData* VisibilityTaskData = OnRenderBegin(GraphBuilder);

	FRDGExternalAccessQueue ExternalAccessQueue;

	{
		static auto CVarDistanceFieldShadowQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DFShadowQuality"));

		if (IsMobileDistanceFieldEnabled(ShaderPlatform)
			&& ViewFamily.EngineShowFlags.Lighting
			&& !Views[0].bIsReflectionCapture
			&& !Views[0].bIsPlanarReflection
			&& !ViewFamily.EngineShowFlags.VisualizeLightCulling
			&& !ViewFamily.UseDebugViewPS()
			&& (CVarDistanceFieldShadowQuality != nullptr && CVarDistanceFieldShadowQuality->GetInt() > 0)
			&& bRendererOutputFinalSceneColor)
		{
			PrepareDistanceFieldScene(GraphBuilder, ExternalAccessQueue);
		}
	}

	ExternalAccessQueue.Submit(GraphBuilder);

	GPU_MESSAGE_SCOPE(GraphBuilder);

	// Establish scene primitive count (must be done after UpdateAllPrimitiveSceneInfos)
	FGPUSceneScopeBeginEndHelper GPUSceneScopeBeginEndHelper(GraphBuilder, Scene->GPUScene, GPUSceneDynamicContext);

	if (bRendererOutputFinalSceneColor)
	{
		if (ShouldRenderSkyAtmosphere(Scene, ViewFamily.EngineShowFlags))
		{
			for (int32 LightIndex = 0; LightIndex < NUM_ATMOSPHERE_LIGHTS; ++LightIndex)
			{
				if (Scene->AtmosphereLights[LightIndex])
				{
					PrepareSunLightProxy(*Scene->GetSkyAtmosphereSceneInfo(), LightIndex, *Scene->AtmosphereLights[LightIndex]);
				}
			}
		}
		else
		{
			Scene->ResetAtmosphereLightsProperties();
		}
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderOther);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FMobileSceneRenderer_Render);

	FSceneTexturesConfig& SceneTexturesConfig = GetActiveSceneTexturesConfig();

	// Initialize global system textures (pass-through if already initialized).
	GSystemTextures.InitializeTextures(GraphBuilder.RHICmdList, FeatureLevel);

	FRDGSystemTextures::Create(GraphBuilder);

	ShaderPrint::BeginViews(GraphBuilder, Views);

	ON_SCOPE_EXIT
	{
		ShaderPrint::EndViews(Views);
	};

	TUniquePtr<FVirtualTextureUpdater> VirtualTextureUpdater;

	if (bUseVirtualTexturing)
	{
		FVirtualTextureUpdateSettings Settings;
		Settings.EnableThrottling(!ViewFamily.bOverrideVirtualTextureThrottle);

		VirtualTextureUpdater = FVirtualTextureSystem::Get().BeginUpdate(GraphBuilder, FeatureLevel, Scene, Settings);
		VirtualTextureFeedbackBegin(GraphBuilder, Views, SceneTexturesConfig.Extent);
	}

	// Substrate initialization is always run even when not enabled.
	if (Substrate::IsSubstrateEnabled())
	{
		for (FViewInfo& View : Views)
		{
			ShadingEnergyConservation::Init(GraphBuilder, View);

			FGlintShadingLUTsStateData::Init(GraphBuilder, View);
		}
	}
	Substrate::InitialiseSubstrateFrameSceneData(GraphBuilder, *this);

	if (bRendererOutputFinalSceneColor)
	{
		// Force the subsurface profile texture to be updated.
		UpdateSubsurfaceProfileTexture(GraphBuilder, ShaderPlatform);
		SpecularProfileAtlas::UpdateSpecularProfileTextureAtlas(GraphBuilder, ShaderPlatform);

		if (bDeferredShading)
		{
			RectLightAtlas::UpdateAtlasTexture(GraphBuilder, FeatureLevel);
			IESAtlas::UpdateAtlasTexture(GraphBuilder, ShaderPlatform);
		}

		// Important that this uses consistent logic throughout the frame, so evaluate once and pass in the flag from here
		// NOTE: Must be done after  system texture initialization
		VirtualShadowMapArray.Initialize(GraphBuilder, Scene->GetVirtualShadowMapCache(), UseVirtualShadowMaps(ShaderPlatform, FeatureLevel), ViewFamily.EngineShowFlags);
	}

	FInitViewTaskDatas InitViewTaskDatas(VisibilityTaskData);

	GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_InitViews));

	// Find the visible primitives and prepare targets and buffers for rendering
	FInstanceCullingManager& InstanceCullingManager = *GraphBuilder.AllocObject<FInstanceCullingManager>(GetSceneUniforms(), Scene->GPUScene.IsEnabled(), GraphBuilder);
	InitViews(GraphBuilder, SceneTexturesConfig, InstanceCullingManager, VirtualTextureUpdater.Get(), InitViewTaskDatas);

	GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_AfterInitViews));

	if (bRendererOutputFinalSceneColor)
	{
		BeginOcclusionScope(GraphBuilder, Views);
	}
	
	if (bIsFirstSceneRenderer)
	{
		GraphBuilder.SetFlushResourcesRHI();
	}

	// Allow scene extensions to affect the scene uniform buffer
	GetSceneExtensionsRenderers().UpdateSceneUniformBuffer(GraphBuilder, GetSceneUniforms());

	GetSceneExtensionsRenderers().PreRender(GraphBuilder);
	GEngine->GetPreRenderDelegateEx().Broadcast(GraphBuilder);
	
	GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_SceneSim));

	FSceneTextures::InitializeViewFamily(GraphBuilder, ViewFamily);
	FSceneTextures& SceneTextures = GetActiveSceneTextures();

	FSortedLightSetSceneInfo& SortedLightSet = *GraphBuilder.AllocObject<FSortedLightSetSceneInfo>();

	SceneTextures.MobileSetupMode = EMobileSceneTextureSetupMode::None;
	SceneTextures.MobileUniformBuffer = CreateMobileSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, SceneTextures.MobileSetupMode);

	const bool bUseHalfResLocalFogVolume = bIsFullDepthPrepassEnabled && IsLocalFogVolumeHalfResolution(); // We must have a full depth buffer in order to render half res and upsample

	if (bRendererOutputFinalSceneColor)
	{
#if WITH_DEBUG_VIEW_MODES
		if (ViewFamily.UseDebugViewPS() && ViewFamily.EngineShowFlags.ShaderComplexity && SceneTextures.QuadOverdraw)
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(SceneTextures.QuadOverdraw), FUintVector4(0, 0, 0, 0));
		}
#endif

		if (bUseVirtualTexturing)
		{
			FVirtualTextureSystem::Get().EndUpdate(GraphBuilder, MoveTemp(VirtualTextureUpdater), FeatureLevel);
		}

		if (bDeferredShading ||
			bEnableClusteredLocalLights ||
			bEnableClusteredReflections)
		{
			RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, SortLights);
			// Shadows are applied in clustered shading on mobile forward and separately on mobile deferred.
			bool bShadowedLightsInClustered = bRequiresShadowProjections && !bDeferredShading;
			GatherAndSortLights(SortedLightSet, bShadowedLightsInClustered);
			int32 NumReflectionCaptures = Views[0].NumBoxReflectionCaptures + Views[0].NumSphereReflectionCaptures;
			bool bCullLightsToGrid = (((bEnableClusteredReflections || bDeferredShading) && NumReflectionCaptures > 0) || bEnableClusteredLocalLights);
			if (bCullLightsToGrid)
			{
				ComputeLightGrid(GraphBuilder, bEnableClusteredLocalLights, SortedLightSet);
			}

			LightFunctionAtlas.RenderLightFunctionAtlas(GraphBuilder, Views);
		}
		else
		{
			SetDummyForwardLightUniformBufferOnViews(GraphBuilder, ShaderPlatform, Views);
		}

		// Notify the FX system that the scene is about to be rendered.
		// TODO: These should probably be moved to scene extensions
		if (FXSystem)
		{
			FXSystem->PreRender(GraphBuilder, GetSceneViews(), GetSceneUniforms(), true /*bAllowGPUParticleUpdate*/);
			if (FGPUSortManager* GPUSortManager = FXSystem->GetGPUSortManager())
			{
				// if GPUSortManager::OnPostRenderOpaque is called below (from RenderOpaqueFX) we must also call OnPreRender (as it sets up
				// the internal state of the GPUSortManager).  Any optimization to skip this block needs to take that into consideration.
				GPUSortManager->OnPreRender(GraphBuilder);
			}
		}

		// Generate the Sky/Atmosphere look up tables
		const bool bShouldRenderSkyAtmosphere = ShouldRenderSkyAtmosphere(Scene, ViewFamily.EngineShowFlags);
		if (bShouldRenderSkyAtmosphere)
		{
			FSkyAtmospherePendingRDGResources PendingRDGResources;
			RenderSkyAtmosphereLookUpTables(GraphBuilder, /* out */ PendingRDGResources);
			PendingRDGResources.CommitToSceneAndViewUniformBuffers(GraphBuilder, /* out */ ExternalAccessQueue);
		}

		// Hair update
		if (IsHairStrandsEnabled(EHairStrandsShaderType::All, Scene->GetShaderPlatform()) && RendererOutput == ERendererOutput::FinalSceneColor)
		{
			FHairStrandsBookmarkParameters& HairStrandsBookmarkParameters = *GraphBuilder.AllocObject<FHairStrandsBookmarkParameters>();
			CreateHairStrandsBookmarkParameters(Scene, Views, AllFamilyViews, HairStrandsBookmarkParameters);
			check(Scene->HairStrandsSceneData.TransientResources);
			HairStrandsBookmarkParameters.TransientResources = Scene->HairStrandsSceneData.TransientResources;

			// Not need for hair uniform buffer, as this is only used for strands rendering
			// If some shader refers to it, we can create a default one with HairStrands::CreateDefaultHairStrandsViewUniformBuffer(GraphBuilder, View);
			for (FViewInfo& View : Views)
			{
				View.HairStrandsViewData.UniformBuffer = nullptr;
			}

			// Interpolation needs to happen after the skin cache run as there is a dependency 
			// on the skin cache output.
			const bool bRunHairStrands = HairStrandsBookmarkParameters.HasInstances() && (Views.Num() > 0);
			if (bRunHairStrands)
			{
				// 1. Update groom visible in primary views
				RunHairStrandsBookmark(GraphBuilder, EHairStrandsBookmark::ProcessCardsAndMeshesInterpolation_PrimaryView, HairStrandsBookmarkParameters);

				// 2. Update groom only visible in shadow 
				// For now, not running on mobile to keep computation light
				// UpdateHairStrandsBookmarkParameters(Scene, Views, HairStrandsBookmarkParameters);
				// RunHairStrandsBookmark(GraphBuilder, EHairStrandsBookmark::ProcessCardsAndMeshesInterpolation_ShadowView, HairStrandsBookmarkParameters);
			}
		}

		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_Shadows));
		RenderShadowDepthMaps(GraphBuilder, nullptr, InstanceCullingManager, ExternalAccessQueue);
		GraphBuilder.AddDispatchHint();

		// Run local fog volume initialization before base pass and volumetric fog for all the culled instance instance data to be ready.
		InitLocalFogVolumesForViews(Scene, Views, ViewFamily, GraphBuilder, ShouldRenderVolumetricFog(), bUseHalfResLocalFogVolume);

		if (ShouldRenderVolumetricFog())
		{
			ComputeVolumetricFog(GraphBuilder, SceneTextures);
		}
		ExternalAccessQueue.Submit(GraphBuilder);

		PollOcclusionQueriesPass(GraphBuilder);

		// Custom depth
		// bShouldRenderCustomDepth has been initialized in InitViews on mobile platform
		if (bShouldRenderCustomDepth)
		{
			RenderCustomDepthPass(GraphBuilder, SceneTextures.CustomDepth, SceneTextures.GetSceneTextureShaderParameters(FeatureLevel), {}, {});
		}
	}
	else
	{
		SetDummyLocalFogVolumeForViews(GraphBuilder, Views);
	}
	
	// Sort objects' triangles
	for (FViewInfo& View : Views)
	{
		if (View.ShouldRenderView() && OIT::IsSortedTrianglesEnabled(View.GetShaderPlatform()))
		{
			OIT::AddSortTrianglesPass(GraphBuilder, View, Scene->OITSceneData, FTriangleSortingOrder::BackToFront);
		}
	}

	FRDGTextureRef ViewFamilyTexture = TryCreateViewFamilyTexture(GraphBuilder, ViewFamily);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	RenderWaterInfoTexture(GraphBuilder, *this, Scene);
	
	if (CustomRenderPassInfos.Num() > 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_CustomRenderPasses);
		RDG_EVENT_SCOPE(GraphBuilder, "CustomRenderPasses");
		RDG_GPU_STAT_SCOPE(GraphBuilder, CustomRenderPasses);

		for (int32 i = 0; i < CustomRenderPassInfos.Num(); ++i)
		{
			FCustomRenderPassBase* CustomRenderPass = CustomRenderPassInfos[i].CustomRenderPass;
			TArray<FViewInfo>& CustomRenderPassViews = CustomRenderPassInfos[i].Views;
			check(CustomRenderPass);

			CustomRenderPass->BeginPass(GraphBuilder);

			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_CustomRenderPass);
				RDG_EVENT_SCOPE(GraphBuilder, "CustomRenderPass[%d] %s", i, *CustomRenderPass->GetDebugName());

				CustomRenderPass->PreRender(GraphBuilder);

				// Setup dummy uniform buffer parameters for fog volume.
				SetDummyLocalFogVolumeForViews(GraphBuilder, CustomRenderPassViews);

				if (bIsFullDepthPrepassEnabled)
				{
					RenderFullDepthPrepass(GraphBuilder, CustomRenderPassViews, SceneTextures, true);
					if (!bRequiresSceneDepthAux)
					{
						AddResolveSceneDepthPass(GraphBuilder, CustomRenderPassViews, SceneTextures.Depth);
					}
				}

				// Render base pass if the custom pass requires it. Otherwise if full depth prepass is not enabled, then depth is generated in the base pass.
				if (CustomRenderPass->GetRenderMode() == FCustomRenderPassBase::ERenderMode::DepthAndBasePass || (CustomRenderPass->GetRenderMode() == FCustomRenderPassBase::ERenderMode::DepthPass && !bIsFullDepthPrepassEnabled))
				{
					RenderCustomRenderPassBasePass(GraphBuilder, CustomRenderPassViews, ViewFamilyTexture, SceneTextures);
				}

				SceneTextures.MobileSetupMode = EMobileSceneTextureSetupMode::SceneColor | EMobileSceneTextureSetupMode::SceneDepth | EMobileSceneTextureSetupMode::SceneDepthAux;
				SceneTextures.MobileUniformBuffer = CreateMobileSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, SceneTextures.MobileSetupMode);

				CopySceneCaptureComponentToTarget(GraphBuilder, SceneTextures, CustomRenderPass->GetRenderTargetTexture(), ViewFamily, CustomRenderPassViews);

				CustomRenderPass->PostRender(GraphBuilder);
			}

			CustomRenderPass->EndPass(GraphBuilder);
		}
	}

	FDBufferTextures DBufferTextures{};
	if (bIsFullDepthPrepassEnabled)
	{
		RenderFullDepthPrepass(GraphBuilder, Views, SceneTextures);

		if (!bRequiresSceneDepthAux)
		{
			AddResolveSceneDepthPass(GraphBuilder, Views, SceneTextures.Depth);
		}

		SceneTextures.MobileSetupMode = EMobileSceneTextureSetupMode::SceneDepth;
		SceneTextures.MobileUniformBuffer = CreateMobileSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, SceneTextures.MobileSetupMode);

		// When renderer is in ERendererOutput::DepthPrepassOnly mode, bRequiresShadowProjections is set to false in InitViews()
		if (bRequiresShadowProjections)
		{
			RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderMobileShadowProjections);
			RDG_GPU_STAT_SCOPE(GraphBuilder, ShadowProjection);
			RenderMobileShadowProjections(GraphBuilder);
		}

		// When renderer is in ERendererOutput::DepthPrepassOnly mode, bShouldRenderHZB is set to false in InitViews()
		if (bShouldRenderHZB)
		{
			RenderHZB(GraphBuilder, SceneTextures.Depth.Resolve);
		}

		// When renderer is in ERendererOutput::DepthPrepassOnly mode, bRequiresAmbientOcclusionPass is set to false in InitViews()
		if (bRequiresAmbientOcclusionPass)
		{
			RenderAmbientOcclusion(GraphBuilder, SceneTextures.Depth.Resolve, SceneTextures.ScreenSpaceAO);
		}

		// Local Light prepass

		if (bRendererOutputFinalSceneColor)
		{
			RenderMobileLocalLightsBuffer(GraphBuilder, SceneTextures, SortedLightSet);
		}

		if (bRendererOutputFinalSceneColor)
		{
			if (bRequiresDBufferDecals)
			{
				DBufferTextures = CreateDBufferTextures(GraphBuilder, SceneTextures.Config.Extent, ShaderPlatform);
				RenderDBuffer(GraphBuilder, SceneTextures, DBufferTextures, InstanceCullingManager);
			}
		}

		// Render half res local fog volume here
		for (FViewInfo& View : Views)
		{
			if (View.LocalFogVolumeViewData.bUseHalfResLocalFogVolume)
			{
				RenderLocalFogVolumeHalfResMobile(GraphBuilder, View);
			}
		}
	}

	for (FSceneViewExtensionRef& ViewExtension : ViewFamily.ViewExtensions)
	{
		ViewExtension->PreRenderBasePass_RenderThread(GraphBuilder, bIsFullDepthPrepassEnabled /*bDepthBufferIsPopulated*/);
	}

	if (bRendererOutputFinalSceneColor)
	{
		if (bDeferredShading)
		{
			RenderDeferred(GraphBuilder, SortedLightSet, ViewFamilyTexture, SceneTextures);
		}
		else
		{
			RenderForward(GraphBuilder, ViewFamilyTexture, SceneTextures, DBufferTextures);
		}

		EndOcclusionScope(GraphBuilder, Views);

		if (!bIsFullDepthPrepassEnabled)
		{
			FenceOcclusionTests(GraphBuilder);
		}

		SceneTextures.MobileSetupMode = EMobileSceneTextureSetupMode::All;
		SceneTextures.MobileSetupMode &= ~EMobileSceneTextureSetupMode::SceneVelocity;
		SceneTextures.MobileUniformBuffer = CreateMobileSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, SceneTextures.MobileSetupMode);

		if (bShouldRenderVelocities)
		{
			// Render the velocities of movable objects
			GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_Velocity));
			RenderVelocities(GraphBuilder, Views, SceneTextures, EVelocityPass::Opaque, false);

			GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_TranslucentVelocity));
			RenderVelocities(GraphBuilder, Views, SceneTextures, EVelocityPass::Translucent, false);

			SceneTextures.MobileSetupMode = EMobileSceneTextureSetupMode::All;
			SceneTextures.MobileUniformBuffer = CreateMobileSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, SceneTextures.MobileSetupMode);
		}
	
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_Post));

		FRendererModule& RendererModule = static_cast<FRendererModule&>(GetRendererModule());
		RendererModule.RenderPostOpaqueExtensions(GraphBuilder, Views, SceneTextures);

		RenderOpaqueFX(GraphBuilder, GetSceneViews(), GetSceneUniforms(), FXSystem, SceneTextures.MobileUniformBuffer);

		if (bRequiresPixelProjectedPlanarRelfectionPass)
		{
			const FPlanarReflectionSceneProxy* PlanarReflectionSceneProxy = Scene ? Scene->GetForwardPassGlobalPlanarReflection() : nullptr;

			RenderPixelProjectedReflection(GraphBuilder, SceneTextures.Color.Resolve, SceneTextures.Depth.Resolve, SceneTextures.PixelProjectedReflection, PlanarReflectionSceneProxy);
		}

		if (ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields || ViewFamily.EngineShowFlags.VisualizeGlobalDistanceField)
		{
			RenderMeshDistanceFieldVisualization(GraphBuilder, SceneTextures);
		}
	
		if (ViewFamily.EngineShowFlags.VisualizeInstanceOcclusionQueries
			&& Scene->InstanceCullingOcclusionQueryRenderer)
		{
			for (FViewInfo& View : Views)
			{
				Scene->InstanceCullingOcclusionQueryRenderer->RenderDebug(GraphBuilder, Scene->GPUScene, View, SceneTextures);
			}
		}

		if (bUseVirtualTexturing)
		{
			RDG_GPU_STAT_SCOPE(GraphBuilder, VirtualTextureUpdate);
			VirtualTextureFeedbackEnd(GraphBuilder);
		}
	
		if (ViewFamily.bResolveScene)
		{
			if (bRenderToSceneColor && !bTonemapSubpassInline)
			{
				// Finish rendering for each view, or the full stereo buffer if enabled
				{
					RDG_EVENT_SCOPE(GraphBuilder, "PostProcessing");
					RDG_GPU_STAT_SCOPE(GraphBuilder, Postprocessing);
					SCOPE_CYCLE_COUNTER(STAT_FinishRenderViewTargetTime);

					FMobilePostProcessingInputs PostProcessingInputs;
					PostProcessingInputs.ViewFamilyTexture = ViewFamilyTexture;
					PostProcessingInputs.SceneTextures = CreateMobileSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, EMobileSceneTextureSetupMode::All);
					PostProcessingInputs.bRequiresMultiPass = bRequiresMultiPass;

					for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						if (Views[ViewIndex].ShouldRenderView())
						{
							RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);
							if (bTonemapSubpass)
							{
								AddMobileCustomResolvePass(GraphBuilder, Views[ViewIndex], SceneTextures, ViewFamilyTexture);
							}
							else
							{ 
								AddMobilePostProcessingPasses(GraphBuilder, Scene, Views[ViewIndex], GetSceneUniforms(), PostProcessingInputs, InstanceCullingManager);
							}
						}
					}
				}
			}
		}
	}

	GEngine->GetPostRenderDelegateEx().Broadcast(GraphBuilder);
	GetSceneExtensionsRenderers().PostRender(GraphBuilder);

	GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLMM_SceneEnd));

	OnRenderFinish(GraphBuilder, ViewFamilyTexture);

	if (bRendererOutputFinalSceneColor)
	{
		PollOcclusionQueriesPass(GraphBuilder);
	}

	QueueSceneTextureExtractions(GraphBuilder, SceneTextures);

	if (Scene->InstanceCullingOcclusionQueryRenderer)
	{
		Scene->InstanceCullingOcclusionQueryRenderer->EndFrame(GraphBuilder);
	}
}

void FMobileSceneRenderer::BuildInstanceCullingDrawParams(FRDGBuilder& GraphBuilder, FViewInfo& View, FMobileRenderPassParameters* PassParameters)
{
	if (Scene->GPUScene.IsEnabled())
	{
		if (!bIsFullDepthPrepassEnabled)
		{
			View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, DepthPassInstanceCullingDrawParams);
		}
		View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);
		View.ParallelMeshDrawCommandPasses[EMeshPass::SkyPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, SkyPassInstanceCullingDrawParams);
		View.ParallelMeshDrawCommandPasses[StandardTranslucencyMeshPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, TranslucencyInstanceCullingDrawParams);
		View.ParallelMeshDrawCommandPasses[EMeshPass::DebugViewMode].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, DebugViewModeInstanceCullingDrawParams);
	}
}

FRenderTargetBindingSlots FMobileSceneRenderer::InitRenderTargetBindings_Forward(FRDGTextureRef ViewFamilyTexture, FSceneTextures& SceneTextures)
{
	FRDGTextureRef SceneColor = nullptr;
	FRDGTextureRef SceneColorResolve = nullptr;
	FRDGTextureRef SceneDepth = nullptr;

	// Verify using both MSAA sample count AND the scene color surface sample count, since on GLES you can't have MSAA color targets,
	// so the color target would be created without MSAA, and MSAA is achieved through magical means (the framebuffer, being MSAA,
	// tells the GPU "execute this renderpass as MSAA, and when you're done, automatically resolve and copy into this non-MSAA texture").
	bool bMobileMSAA = NumMSAASamples > 1;

	if (!bRenderToSceneColor)
	{
		if (bMobileMSAA)
		{
			SceneColor = SceneTextures.Color.Target;
			SceneColorResolve = ViewFamilyTexture;
		}
		else
		{
			SceneColor = ViewFamilyTexture;
		}
		SceneDepth = SceneTextures.Depth.Target;
	}
	else
	{
		SceneColor = SceneTextures.Color.Target;
		SceneColorResolve = bMobileMSAA ? SceneTextures.Color.Resolve : nullptr;
		SceneDepth = SceneTextures.Depth.Target;
	}

	FRenderTargetBindingSlots BasePassRenderTargets;
	BasePassRenderTargets[0] = FRenderTargetBinding(SceneColor, SceneColorResolve, ERenderTargetLoadAction::EClear);
	if (bRequiresSceneDepthAux)
	{
		BasePassRenderTargets[1] = FRenderTargetBinding(SceneTextures.DepthAux.Target, SceneTextures.DepthAux.Resolve, ERenderTargetLoadAction::EClear);
	}
		
	if (bTonemapSubpassInline)
	{
		// DepthAux is not used with tonemap subpass, since there are no post-processing passes
		// Backbuffer surface provided as a second render target instead of resolve target.
		BasePassRenderTargets[0].SetResolveTexture(nullptr);
		BasePassRenderTargets[1] = FRenderTargetBinding(ViewFamilyTexture, nullptr, ERenderTargetLoadAction::EClear);
	}
	
	BasePassRenderTargets.DepthStencil = bIsFullDepthPrepassEnabled ? 
		FDepthStencilBinding(SceneDepth, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite) : 
		FDepthStencilBinding(SceneDepth, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	BasePassRenderTargets.SubpassHint = ESubpassHint::None;
	BasePassRenderTargets.NumOcclusionQueries = 0u;

	return BasePassRenderTargets;
}

void FMobileSceneRenderer::RenderForward(FRDGBuilder& GraphBuilder, FRDGTextureRef ViewFamilyTexture, FSceneTextures& SceneTextures, FDBufferTextures& DBufferTextures)
{
	const FViewInfo& MainView = Views[0];

	GVRSImageManager.PrepareImageBasedVRS(GraphBuilder, ViewFamily, SceneTextures);
	FRDGTextureRef NewShadingRateTarget = GVRSImageManager.GetVariableRateShadingImage(GraphBuilder, MainView, FVariableRateShadingImageManager::EVRSPassType::BasePass);

	FRenderTargetBindingSlots BasePassRenderTargets = InitRenderTargetBindings_Forward(ViewFamilyTexture, SceneTextures);
	BasePassRenderTargets.ShadingRateTexture = (!MainView.bIsSceneCapture && !MainView.bIsReflectionCapture && (NewShadingRateTarget != nullptr)) ? NewShadingRateTarget : nullptr;

	static const auto CVarMobileMultiView = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView"));
	const bool bIsMultiViewApplication = (CVarMobileMultiView && CVarMobileMultiView->GetValueOnAnyThread() != 0);

	//if the scenecolor isn't multiview but the app is, need to render as a single-view multiview due to shaders
	BasePassRenderTargets.MultiViewCount = MainView.bIsMobileMultiViewEnabled ? 2 : (bIsMultiViewApplication ? 1 : 0);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FRenderViewContextArray RenderViews;
	GetRenderViews(Views, RenderViews);

	for (FRenderViewContext& ViewContext : RenderViews)
	{
		FViewInfo& View = *ViewContext.ViewInfo;

		SCOPED_GPU_MASK(GraphBuilder.RHICmdList, !View.IsInstancedStereoPass() ? View.GPUMask : (View.GPUMask | View.GetInstancedView()->GPUMask));
		SCOPED_CONDITIONAL_DRAW_EVENTF(GraphBuilder.RHICmdList, EventView, RenderViews.Num() > 1, TEXT("View%d"), ViewContext.ViewIndex);

		if (!ViewContext.bIsFirstView)
		{
			BasePassRenderTargets[0].SetLoadAction(ERenderTargetLoadAction::ELoad);
			if (bRequiresSceneDepthAux)
			{
				BasePassRenderTargets[1].SetLoadAction(ERenderTargetLoadAction::ELoad);
			}
			BasePassRenderTargets.DepthStencil.SetDepthLoadAction(ERenderTargetLoadAction::ELoad);
			BasePassRenderTargets.DepthStencil.SetStencilLoadAction(ERenderTargetLoadAction::ELoad);
			BasePassRenderTargets.DepthStencil.SetDepthStencilAccess(bIsFullDepthPrepassEnabled ? FExclusiveDepthStencil::DepthRead_StencilWrite : FExclusiveDepthStencil::DepthWrite_StencilWrite);
		}

		View.BeginRenderView();

		UpdateDirectionalLightUniformBuffers(GraphBuilder, View);

		FMobileBasePassTextures MobileBasePassTextures{};
		MobileBasePassTextures.ScreenSpaceAO = bRequiresAmbientOcclusionPass ? SceneTextures.ScreenSpaceAO : SystemTextures.White;
		MobileBasePassTextures.DBufferTextures = DBufferTextures;

		EMobileSceneTextureSetupMode SetupMode = (bIsFullDepthPrepassEnabled ? EMobileSceneTextureSetupMode::SceneDepth : EMobileSceneTextureSetupMode::None) | EMobileSceneTextureSetupMode::CustomDepth;
		FMobileRenderPassParameters* PassParameters = GraphBuilder.AllocParameters<FMobileRenderPassParameters>();
		PassParameters->View = View.GetShaderParameters();
		PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Opaque, SetupMode, MobileBasePassTextures);
		PassParameters->ReflectionCapture = View.MobileReflectionCaptureUniformBuffer;
		PassParameters->RenderTargets = BasePassRenderTargets;
		PassParameters->LocalFogVolumeInstances = View.LocalFogVolumeViewData.GPUInstanceDataBufferSRV;
		PassParameters->LocalFogVolumeTileDrawIndirectBuffer = View.LocalFogVolumeViewData.GPUTileDrawIndirectBuffer;
		PassParameters->LocalFogVolumeTileDataTexture = View.LocalFogVolumeViewData.TileDataTextureArraySRV;
		PassParameters->LocalFogVolumeTileDataBuffer = View.LocalFogVolumeViewData.GPUTileDataBufferSRV;
		PassParameters->HalfResLocalFogVolumeViewSRV = View.LocalFogVolumeViewData.HalfResLocalFogVolumeViewSRV;
		PassParameters->HalfResLocalFogVolumeDepthSRV = View.LocalFogVolumeViewData.HalfResLocalFogVolumeDepthSRV;
	
		BuildInstanceCullingDrawParams(GraphBuilder, View, PassParameters);

		// Split if we need to render translucency in a separate render pass
		if (bRequiresMultiPass)
		{
			RenderForwardMultiPass(GraphBuilder, PassParameters, ViewContext, SceneTextures);
		}
		else
		{
			RenderForwardSinglePass(GraphBuilder, PassParameters, ViewContext, SceneTextures);
		}
	}
}

void FMobileSceneRenderer::RenderForwardSinglePass(FRDGBuilder& GraphBuilder, FMobileRenderPassParameters* PassParameters, FRenderViewContext& ViewContext, FSceneTextures& SceneTextures)
{
	if (bTonemapSubpassInline)
	{
		// tonemapping LUT pass before we start main render pass. The texture is needed by the custom resolve pass which does tonemapping
		PassParameters->ColorGradingLUT = AddCombineLUTPass(GraphBuilder, *ViewContext.ViewInfo);
	}
		
	PassParameters->RenderTargets.SubpassHint = bTonemapSubpassInline ? ESubpassHint::CustomResolveSubpass : ESubpassHint::DepthReadSubpass;
	const bool bDoOcclusionQueries = (!bIsFullDepthPrepassEnabled && ViewContext.bIsLastView && DoOcclusionQueries());
	PassParameters->RenderTargets.NumOcclusionQueries = bDoOcclusionQueries ? ComputeNumOcclusionQueriesToBatch() : 0u;
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SceneColorRendering"),
		PassParameters,
		// the second view pass should not be merged with the first view pass on mobile since the subpass would not work properly.
		ERDGPassFlags::Raster | ERDGPassFlags::NeverMerge,
		[this, PassParameters, ViewContext, bDoOcclusionQueries, &SceneTextures](FRHICommandList& RHICmdList)
	{
		FViewInfo& View = *ViewContext.ViewInfo;
			
		if (GIsEditor && !View.bIsSceneCapture && ViewContext.bIsFirstView)
		{
			DrawClearQuad(RHICmdList, View.BackgroundColor);
		}

		// Depth pre-pass
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_MobilePrePass));
		RenderMaskedPrePass(RHICmdList, View);
		// Opaque and masked
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Opaque));
		RenderMobileBasePass(RHICmdList, View, &PassParameters->InstanceCullingDrawParams);
		RenderMobileDebugView(RHICmdList, View);
		RHICmdList.PollOcclusionQueries();
		PostRenderBasePass(RHICmdList, View);
		// scene depth is read only and can be fetched
		RHICmdList.NextSubpass();
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Translucency));
		RenderDecals(RHICmdList, View, &PassParameters->InstanceCullingDrawParams);
		RenderModulatedShadowProjections(RHICmdList, ViewContext.ViewIndex, View);
		if (GMaxRHIShaderPlatform != SP_METAL_SIM)
		{
			RenderFog(RHICmdList, View);
		}
		// Draw translucency.
		RenderTranslucency(RHICmdList, View);
		
#if UE_ENABLE_DEBUG_DRAWING
		if ((!IsMobileHDR() || bTonemapSubpass) && FSceneRenderer::ShouldCompositeDebugPrimitivesInPostProcess(View))
		{
			// Draw debug primitives after translucency for LDR as we do not have a post processing pass
			RenderMobileDebugPrimitives(RHICmdList, View);
		}
#endif

		if (bDoOcclusionQueries)
		{
			// Issue occlusion queries
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Occlusion));
			const bool bAdrenoOcclusionMode = (CVarMobileAdrenoOcclusionMode.GetValueOnRenderThread() != 0 && IsOpenGLPlatform(ShaderPlatform));
			if (bAdrenoOcclusionMode)
			{
				// flush
				RHICmdList.SubmitCommandsHint();
			}
			RenderOcclusion(RHICmdList);
		}

		// Pre-tonemap before MSAA resolve (iOS only)
		PreTonemapMSAA(RHICmdList, SceneTextures);
		if (bTonemapSubpassInline)
		{
			RHICmdList.NextSubpass();
			RenderMobileCustomResolve(RHICmdList, View, NumMSAASamples, SceneTextures);
		}
	});
	
	// resolve MSAA depth
	if (!bIsFullDepthPrepassEnabled)
	{
		AddResolveSceneDepthPass(GraphBuilder, *ViewContext.ViewInfo, SceneTextures.Depth);
	}
}

void FMobileSceneRenderer::RenderForwardMultiPass(FRDGBuilder& GraphBuilder, FMobileRenderPassParameters* PassParameters, FRenderViewContext& ViewContext, FSceneTextures& SceneTextures)
{
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SceneColorRendering"),
		PassParameters,
		ERDGPassFlags::Raster,
		[this, PassParameters, ViewContext, &SceneTextures](FRHICommandList& RHICmdList)
	{
		FViewInfo& View = *ViewContext.ViewInfo;
			
		if (GIsEditor && !View.bIsSceneCapture && ViewContext.bIsFirstView)
		{
			DrawClearQuad(RHICmdList, View.BackgroundColor);
		}

		// Depth pre-pass
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_MobilePrePass));
		RenderMaskedPrePass(RHICmdList, View);
		// Opaque and masked
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Opaque));
		RenderMobileBasePass(RHICmdList, View, &PassParameters->InstanceCullingDrawParams);
		RenderMobileDebugView(RHICmdList, View);
		RHICmdList.PollOcclusionQueries();
		PostRenderBasePass(RHICmdList, View);
	});

	FViewInfo& View = *ViewContext.ViewInfo;

	// resolve MSAA depth
	if (!bIsFullDepthPrepassEnabled)
	{
		AddResolveSceneDepthPass(GraphBuilder, View, SceneTextures.Depth);
	}
	if (bRequiresSceneDepthAux)
	{
		AddResolveSceneColorPass(GraphBuilder, View, SceneTextures.DepthAux);
	}

	FExclusiveDepthStencil::Type ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilRead;
	if (bModulatedShadowsInUse)
	{
		// FIXME: modulated shadows write to stencil
		ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilWrite;
	}

	EMobileSceneTextureSetupMode SetupMode = EMobileSceneTextureSetupMode::SceneDepth | EMobileSceneTextureSetupMode::SceneDepthAux | EMobileSceneTextureSetupMode::CustomDepth;
	FMobileRenderPassParameters* SecondPassParameters = GraphBuilder.AllocParameters<FMobileRenderPassParameters>();
	*SecondPassParameters = *PassParameters;
	SecondPassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Translucent, SetupMode);
	SecondPassParameters->ReflectionCapture = View.MobileReflectionCaptureUniformBuffer;
	SecondPassParameters->RenderTargets[0].SetLoadAction(ERenderTargetLoadAction::ELoad);
	SecondPassParameters->RenderTargets[1] = FRenderTargetBinding();
	SecondPassParameters->RenderTargets.DepthStencil.SetDepthLoadAction(ERenderTargetLoadAction::ELoad);
	SecondPassParameters->RenderTargets.DepthStencil.SetStencilLoadAction(ERenderTargetLoadAction::ELoad);
	SecondPassParameters->RenderTargets.DepthStencil.SetDepthStencilAccess(ExclusiveDepthStencil);
	
	const bool bDoOcclusionQueries = (!bIsFullDepthPrepassEnabled && ViewContext.bIsLastView && DoOcclusionQueries());
	SecondPassParameters->RenderTargets.NumOcclusionQueries = bDoOcclusionQueries ? ComputeNumOcclusionQueriesToBatch() : 0u;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("DecalsAndTranslucency"),
		SecondPassParameters,
		ERDGPassFlags::Raster,
		[this, SecondPassParameters, ViewContext, bDoOcclusionQueries, &SceneTextures](FRHICommandList& RHICmdList)
	{
		FViewInfo& View = *ViewContext.ViewInfo;
			
		// scene depth is read only and can be fetched
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Translucency));
		RenderDecals(RHICmdList, View, &SecondPassParameters->InstanceCullingDrawParams);
		RenderModulatedShadowProjections(RHICmdList, ViewContext.ViewIndex, View);
		RenderFog(RHICmdList, View);
		// Draw translucency.
		RenderTranslucency(RHICmdList, View);

		if (bDoOcclusionQueries)
		{
			// Issue occlusion queries
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Occlusion));
			RenderOcclusion(RHICmdList);
		}

		// Pre-tonemap before MSAA resolve (iOS only)
		PreTonemapMSAA(RHICmdList, SceneTextures);
	});

	AddResolveSceneColorPass(GraphBuilder, View, SceneTextures.Color);
}

class FMobileDeferredCopyPLSPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileDeferredCopyPLSPS, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform) && IsMobileDeferredShadingEnabled(Parameters.Platform);
	}

	/** Default constructor. */
	FMobileDeferredCopyPLSPS() {}

	/** Initialization constructor. */
	FMobileDeferredCopyPLSPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};

IMPLEMENT_SHADER_TYPE(, FMobileDeferredCopyPLSPS, TEXT("/Engine/Private/MobileDeferredUtils.usf"), TEXT("MobileDeferredCopyPLSPS"), SF_Pixel);

class FMobileDeferredCopyDepthPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileDeferredCopyDepthPS, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform) && IsMobileDeferredShadingEnabled(Parameters.Platform);
	}

	/** Default constructor. */
	FMobileDeferredCopyDepthPS() {}

	/** Initialization constructor. */
	FMobileDeferredCopyDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};

IMPLEMENT_SHADER_TYPE(, FMobileDeferredCopyDepthPS, TEXT("/Engine/Private/MobileDeferredUtils.usf"), TEXT("MobileDeferredCopyDepthPS"), SF_Pixel);

template<class T>
void MobileDeferredCopyBuffer(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	// Shade only MSM_DefaultLit pixels
	uint8 StencilRef = GET_STENCIL_MOBILE_SM_MASK(MSM_DefaultLit);
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI(); // 4 bits for shading models

	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
	TShaderMapRef<T> PixelShader(View.ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
		View.GetSceneTexturesConfig().Extent,
		VertexShader);
}

static bool UsingPixelLocalStorage(const FStaticShaderPlatform ShaderPlatform)
{
	return IsAndroidOpenGLESPlatform(ShaderPlatform) && GSupportsPixelLocalStorage && GSupportsShaderDepthStencilFetch;
}

FColorTargets FMobileSceneRenderer::GetColorTargets_Deferred(FSceneTextures& SceneTextures)
{
	FColorTargets ColorTargets;

	// If we are using GL and don't have FBF support, use PLS
	bool bUsingPixelLocalStorage = UsingPixelLocalStorage(ShaderPlatform);

	if (bUsingPixelLocalStorage)
	{
		ColorTargets.Add(SceneTextures.Color.Target);
	}
	else
	{
		ColorTargets.Add(SceneTextures.Color.Target);
		ColorTargets.Add(SceneTextures.GBufferA);
		ColorTargets.Add(SceneTextures.GBufferB);
		ColorTargets.Add(SceneTextures.GBufferC);
		
		if (MobileUsesExtenedGBuffer(ShaderPlatform))
		{
			ColorTargets.Add(SceneTextures.GBufferD);
		}

		if (bRequiresSceneDepthAux)
		{
			ColorTargets.Add(SceneTextures.DepthAux.Target);
		}
	}

	return ColorTargets;
}

FRenderTargetBindingSlots FMobileSceneRenderer::InitRenderTargetBindings_Deferred(FSceneTextures& SceneTextures, TArray<FRDGTextureRef, TInlineAllocator<6>>& ColorTargets)
{
	TArrayView<FRDGTextureRef> BasePassTexturesView = MakeArrayView(ColorTargets);
	FRenderTargetBindingSlots BasePassRenderTargets = GetRenderTargetBindings(ERenderTargetLoadAction::EClear, BasePassTexturesView);
	BasePassRenderTargets.DepthStencil = bIsFullDepthPrepassEnabled ? 
		FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite) : 
		FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	BasePassRenderTargets.SubpassHint = ESubpassHint::None;
	BasePassRenderTargets.NumOcclusionQueries = 0u;
	BasePassRenderTargets.ShadingRateTexture = nullptr;
	BasePassRenderTargets.MultiViewCount = 0;
	return BasePassRenderTargets;
}

void FMobileSceneRenderer::RenderDeferred(FRDGBuilder& GraphBuilder, const FSortedLightSetSceneInfo& SortedLightSet, FRDGTextureRef ViewFamilyTexture, FSceneTextures& SceneTextures)
{
	FColorTargets ColorTargets = GetColorTargets_Deferred(SceneTextures);
	FRenderTargetBindingSlots BasePassRenderTargets = InitRenderTargetBindings_Deferred(SceneTextures, ColorTargets);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FRenderViewContextArray RenderViews;
	GetRenderViews(Views, RenderViews);

	for (FRenderViewContext& ViewContext : RenderViews)
	{
		FViewInfo& View = *ViewContext.ViewInfo;

		SCOPED_GPU_MASK(GraphBuilder.RHICmdList, !View.IsInstancedStereoPass() ? View.GPUMask : (View.GPUMask | View.GetInstancedView()->GPUMask));
		SCOPED_CONDITIONAL_DRAW_EVENTF(GraphBuilder.RHICmdList, EventView, RenderViews.Num() > 1, TEXT("View%d"), ViewContext.ViewIndex);

		if (!ViewContext.bIsFirstView)
		{
			// Load targets for a non-first view 
			for (int32 i = 0; i < ColorTargets.Num(); ++i)
			{
				BasePassRenderTargets[i].SetLoadAction(ERenderTargetLoadAction::ELoad);
			}
			BasePassRenderTargets.DepthStencil.SetDepthLoadAction(ERenderTargetLoadAction::ELoad);
			BasePassRenderTargets.DepthStencil.SetStencilLoadAction(ERenderTargetLoadAction::ELoad);
			BasePassRenderTargets.DepthStencil.SetDepthStencilAccess(bIsFullDepthPrepassEnabled ? FExclusiveDepthStencil::DepthRead_StencilWrite : FExclusiveDepthStencil::DepthWrite_StencilWrite);
		}

		View.BeginRenderView();

		UpdateDirectionalLightUniformBuffers(GraphBuilder, View);

		FMobileBasePassTextures MobileBasePassTextures{};
		MobileBasePassTextures.ScreenSpaceAO = bRequiresAmbientOcclusionPass ? SceneTextures.ScreenSpaceAO : SystemTextures.White;

		EMobileSceneTextureSetupMode SetupMode = (bIsFullDepthPrepassEnabled ? EMobileSceneTextureSetupMode::SceneDepth : EMobileSceneTextureSetupMode::None) | EMobileSceneTextureSetupMode::CustomDepth;
		auto* PassParameters = GraphBuilder.AllocParameters<FMobileRenderPassParameters>();
		PassParameters->View = View.GetShaderParameters();
		PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Opaque, SetupMode, MobileBasePassTextures);
		PassParameters->ReflectionCapture = View.MobileReflectionCaptureUniformBuffer;
		PassParameters->RenderTargets = BasePassRenderTargets;
		PassParameters->LocalFogVolumeInstances = View.LocalFogVolumeViewData.GPUInstanceDataBufferSRV;
		PassParameters->LocalFogVolumeTileDrawIndirectBuffer = View.LocalFogVolumeViewData.GPUTileDrawIndirectBuffer;
		PassParameters->LocalFogVolumeTileDataTexture = View.LocalFogVolumeViewData.TileDataTextureArraySRV;
		PassParameters->LocalFogVolumeTileDataBuffer = View.LocalFogVolumeViewData.GPUTileDataBufferSRV;
		PassParameters->HalfResLocalFogVolumeViewSRV = View.LocalFogVolumeViewData.HalfResLocalFogVolumeViewSRV;
		PassParameters->HalfResLocalFogVolumeDepthSRV = View.LocalFogVolumeViewData.HalfResLocalFogVolumeDepthSRV;
		
		BuildInstanceCullingDrawParams(GraphBuilder, View, PassParameters);

		if (bRequiresMultiPass)
		{
			RenderDeferredMultiPass(GraphBuilder, PassParameters, ColorTargets.Num(), ViewContext, SceneTextures, SortedLightSet);
		}
		else
		{
			RenderDeferredSinglePass(GraphBuilder, PassParameters, ViewContext, SceneTextures, SortedLightSet, UsingPixelLocalStorage(ShaderPlatform));
		}
	}
}

void FMobileSceneRenderer::RenderDeferredSinglePass(FRDGBuilder& GraphBuilder, class FMobileRenderPassParameters* PassParameters, FRenderViewContext& ViewContext, FSceneTextures& SceneTextures, const FSortedLightSetSceneInfo& SortedLightSet, bool bUsingPixelLocalStorage)
{
	PassParameters->RenderTargets.SubpassHint = ESubpassHint::DeferredShadingSubpass;

	const bool bDoOcclusionQueires = (!bIsFullDepthPrepassEnabled && ViewContext.bIsLastView && DoOcclusionQueries());
	PassParameters->RenderTargets.NumOcclusionQueries = bDoOcclusionQueires ? ComputeNumOcclusionQueriesToBatch() : 0u;
				
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SceneColorRendering"),
		PassParameters,
		// the second view pass should not be merged with the first view pass on mobile since the subpass would not work properly.
		ERDGPassFlags::Raster | ERDGPassFlags::NeverMerge,
		[this, PassParameters, ViewContext, bDoOcclusionQueires, &SceneTextures, &SortedLightSet, bUsingPixelLocalStorage](FRHICommandList& RHICmdList)
	{
		FViewInfo& View = *ViewContext.ViewInfo;
			
		// Depth pre-pass
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_MobilePrePass));
		RenderMaskedPrePass(RHICmdList, View);
		// Opaque and masked
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Opaque));
		RenderMobileBasePass(RHICmdList, View, &PassParameters->InstanceCullingDrawParams);
		RenderMobileDebugView(RHICmdList, View);
		RHICmdList.PollOcclusionQueries();
		PostRenderBasePass(RHICmdList, View);
		// SceneColor + GBuffer write, SceneDepth is read only
		RHICmdList.NextSubpass();
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Translucency));
		RenderDecals(RHICmdList, View, &PassParameters->InstanceCullingDrawParams);
		// SceneColor write, SceneDepth is read only
		RHICmdList.NextSubpass();
		MobileDeferredShadingPass(RHICmdList, ViewContext.ViewIndex, Views.Num(), View, *Scene, SortedLightSet, VisibleLightInfos);
		if (bUsingPixelLocalStorage)
		{
			MobileDeferredCopyBuffer<FMobileDeferredCopyPLSPS>(RHICmdList, View);
		}
		RenderFog(RHICmdList, View);
		// Draw translucency.
		RenderTranslucency(RHICmdList, View);

		if (bDoOcclusionQueires)
		{
			// Issue occlusion queries
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Occlusion));
			RenderOcclusion(RHICmdList);
		}
	});
}

void FMobileSceneRenderer::RenderDeferredMultiPass(FRDGBuilder& GraphBuilder, class FMobileRenderPassParameters* PassParameters, int32 NumColorTargets, FRenderViewContext& ViewContext, FSceneTextures& SceneTextures, const FSortedLightSetSceneInfo& SortedLightSet)
{
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SceneColorRendering"),
		PassParameters,
		ERDGPassFlags::Raster,
		[this, PassParameters, ViewContext, &SceneTextures](FRHICommandList& RHICmdList)
	{
		FViewInfo& View = *ViewContext.ViewInfo;
		
		// Depth pre-pass
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_MobilePrePass));
		RenderMaskedPrePass(RHICmdList, View);
		// Opaque and masked
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Opaque));
		RenderMobileBasePass(RHICmdList, View, &PassParameters->InstanceCullingDrawParams);
		RenderMobileDebugView(RHICmdList, View);
		RHICmdList.PollOcclusionQueries();
		PostRenderBasePass(RHICmdList, View);
	});

	FViewInfo& View = *ViewContext.ViewInfo;
	
	EMobileSceneTextureSetupMode SetupMode = EMobileSceneTextureSetupMode::SceneDepth | EMobileSceneTextureSetupMode::SceneDepthAux | EMobileSceneTextureSetupMode::CustomDepth;
	auto* SecondPassParameters = GraphBuilder.AllocParameters<FMobileRenderPassParameters>();
	*SecondPassParameters = *PassParameters;
	SecondPassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, View, EMobileBasePass::Translucent, SetupMode);
	SecondPassParameters->ReflectionCapture = View.MobileReflectionCaptureUniformBuffer;
	// SceneColor + GBuffer write, SceneDepth is read only
	for (int32 i = 0; i < NumColorTargets; ++i)
	{
		SecondPassParameters->RenderTargets[i].SetLoadAction(ERenderTargetLoadAction::ELoad);
	}
	SecondPassParameters->RenderTargets.DepthStencil.SetDepthLoadAction(ERenderTargetLoadAction::ELoad);
	SecondPassParameters->RenderTargets.DepthStencil.SetStencilLoadAction(ERenderTargetLoadAction::ELoad);
	SecondPassParameters->RenderTargets.DepthStencil.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Decals"),
		SecondPassParameters,
		ERDGPassFlags::Raster,
		[this, SecondPassParameters, ViewContext](FRHICommandList& RHICmdList)
	{
		FViewInfo& View = *ViewContext.ViewInfo;
		RenderDecals(RHICmdList, View, &SecondPassParameters->InstanceCullingDrawParams);
	});

	auto* ThirdPassParameters = GraphBuilder.AllocParameters<FMobileRenderPassParameters>();
	*ThirdPassParameters = *SecondPassParameters;
	// SceneColor write, SceneDepth is read only
	for (int32 i = 1; i < NumColorTargets; ++i)
	{
		ThirdPassParameters->RenderTargets[i] = FRenderTargetBinding();
	}
	ThirdPassParameters->RenderTargets.DepthStencil.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilWrite);
		
	const bool bDoOcclusionQueires = (!bIsFullDepthPrepassEnabled && ViewContext.bIsLastView && DoOcclusionQueries());
	ThirdPassParameters->RenderTargets.NumOcclusionQueries = bDoOcclusionQueires ? ComputeNumOcclusionQueriesToBatch() : 0u;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("LightingAndTranslucency"),
		ThirdPassParameters,
		ERDGPassFlags::Raster,
		[this, ThirdPassParameters, ViewContext, bDoOcclusionQueires, &SceneTextures, &SortedLightSet](FRHICommandList& RHICmdList)
	{
		FViewInfo& View = *ViewContext.ViewInfo;
			
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Translucency));
		MobileDeferredShadingPass(RHICmdList, ViewContext.ViewIndex, Views.Num(), View, *Scene, SortedLightSet, VisibleLightInfos);
		RenderFog(RHICmdList, View);

		// Draw translucency.
		RenderTranslucency(RHICmdList, View);

		if (bDoOcclusionQueires)
		{
			// Issue occlusion queries
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_Occlusion));
			RenderOcclusion(RHICmdList);
		}
	});
}

void FMobileSceneRenderer::PostRenderBasePass(FRHICommandList& RHICmdList, FViewInfo& View)
{
	if (ViewFamily.ViewExtensions.Num() > 1)
	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ViewExtensionPostRenderBasePass);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FMobileSceneRenderer_ViewExtensionPostRenderBasePass);
		for (int32 ViewExt = 0; ViewExt < ViewFamily.ViewExtensions.Num(); ++ViewExt)
		{
			ViewFamily.ViewExtensions[ViewExt]->PostRenderBasePassMobile_RenderThread(RHICmdList, View);
		}
	}
}

void FMobileSceneRenderer::RenderMobileDebugView(FRHICommandList& RHICmdList, const FViewInfo& View)
{
#if WITH_DEBUG_VIEW_MODES
	if (ViewFamily.UseDebugViewPS())
	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderDebugView);
		SCOPED_DRAW_EVENT(RHICmdList, MobileDebugView);
		SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);

		// Here we use the base pass depth result to get z culling for opaque and masque.
		// The color needs to be cleared at this point since shader complexity renders in additive.
		DrawClearQuad(RHICmdList, FLinearColor::Black);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
		View.ParallelMeshDrawCommandPasses[EMeshPass::DebugViewMode].DispatchDraw(nullptr, RHICmdList, &DebugViewModeInstanceCullingDrawParams);
	}
#endif // WITH_DEBUG_VIEW_MODES
}

int32 FMobileSceneRenderer::ComputeNumOcclusionQueriesToBatch() const
{
	int32 NumQueriesForBatch = 0;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		const FSceneViewState* ViewState = (FSceneViewState*)View.State;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!ViewState || (!ViewState->bIsFrozen))
#endif
		{
			NumQueriesForBatch += View.IndividualOcclusionQueries.GetNumBatchOcclusionQueries();
			NumQueriesForBatch += View.GroupedOcclusionQueries.GetNumBatchOcclusionQueries();
		}
	}
	
	return NumQueriesForBatch;
}

// Whether we need a separate render-passes for translucency, decals etc
bool FMobileSceneRenderer::RequiresMultiPass(int32 NumMSAASamples, EShaderPlatform ShaderPlatform)
{
	// Vulkan uses subpasses
	if (IsVulkanPlatform(ShaderPlatform))
	{
		return false;
	}

	// All iOS support frame_buffer_fetch
	if (IsMetalMobilePlatform(ShaderPlatform) && GSupportsShaderFramebufferFetch)
	{
		return false;
	}

	// Some Androids support frame_buffer_fetch
	if (IsAndroidOpenGLESPlatform(ShaderPlatform) && (GSupportsShaderFramebufferFetch || GSupportsShaderDepthStencilFetch))
	{
		return false;
	}

	// Only Vulkan, iOS and some GL can do a single pass deferred shading, otherwise multipass
	if (IsMobileDeferredShadingEnabled(ShaderPlatform))
	{
		return true;
	}
		
	// Always render LDR in single pass
	if (!IsMobileHDR() && !IsSimulatedPlatform(ShaderPlatform))
	{
		return false;
	}

	// MSAA depth can't be sampled or resolved, unless we are on PC (no vulkan)
	if (NumMSAASamples > 1 && !IsSimulatedPlatform(ShaderPlatform))
	{
		return false;
	}

	return true;
}

void FMobileSceneRenderer::UpdateDirectionalLightUniformBuffers(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	if (CachedView == &View)
	{
		return;
	}
	CachedView = &View;

	AddPass(GraphBuilder, RDG_EVENT_NAME("UpdateDirectionalLightUniformBuffers"), [this, &View](FRHICommandListImmediate& RHICmdList)
	{
		const bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows;
		// Fill in the other entries based on the lights
		for (int32 ChannelIdx = 0; ChannelIdx < UE_ARRAY_COUNT(Scene->MobileDirectionalLights); ChannelIdx++)
		{
			FMobileDirectionalLightShaderParameters Params;
			SetupMobileDirectionalLightUniformParameters(*Scene, View, VisibleLightInfos, ChannelIdx, bDynamicShadows, Params);
			Scene->UniformBuffers.MobileDirectionalLightUniformBuffers[ChannelIdx + 1].UpdateUniformBufferImmediate(RHICmdList, Params);
		}
	});
}

void FMobileSceneRenderer::UpdateSkyReflectionUniformBuffer(FRHICommandListBase& RHICmdList)
{
	FSkyLightSceneProxy* SkyLight = nullptr;
	if (Scene->SkyLight
		&& Scene->SkyLight->ProcessedTexture
		&& Scene->SkyLight->ProcessedTexture->TextureRHI
		// Don't use skylight reflection if it is a static sky light for keeping coherence with PC.
		&& !Scene->SkyLight->bHasStaticLighting)
	{
		SkyLight = Scene->SkyLight;
	}

	FMobileReflectionCaptureShaderParameters Parameters;
	SetupMobileSkyReflectionUniformParameters(SkyLight, Parameters);
	Scene->UniformBuffers.MobileSkyReflectionUniformBuffer.UpdateUniformBufferImmediate(RHICmdList, Parameters);
}

class FPreTonemapMSAA_Mobile : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPreTonemapMSAA_Mobile, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMetalMobilePlatform(Parameters.Platform);
	}	

	FPreTonemapMSAA_Mobile() {}

public:
	FPreTonemapMSAA_Mobile(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};

IMPLEMENT_SHADER_TYPE(, FPreTonemapMSAA_Mobile,TEXT("/Engine/Private/PostProcessMobile.usf"),TEXT("PreTonemapMSAA_Mobile"),SF_Pixel);

void FMobileSceneRenderer::PreTonemapMSAA(FRHICommandList& RHICmdList, const FMinimalSceneTextures& SceneTextures)
{
	// iOS only
	bool bOnChipPP = GSupportsRenderTargetFormat_PF_FloatRGBA && GSupportsShaderFramebufferFetch &&	ViewFamily.EngineShowFlags.PostProcessing;
	bool bOnChipPreTonemapMSAA = bOnChipPP && IsMetalMobilePlatform(ViewFamily.GetShaderPlatform()) && (NumMSAASamples > 1);
	if (!bOnChipPreTonemapMSAA || bGammaSpace)
	{
		return;
	}

	const FIntPoint TargetSize = SceneTextures.Config.Extent;

	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FPreTonemapMSAA_Mobile> PixelShader(ShaderMap);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, CW_NONE>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);

	DrawRectangle(
		RHICmdList,
		0, 0,
		TargetSize.X, TargetSize.Y,
		0, 0,
		TargetSize.X, TargetSize.Y,
		TargetSize,
		TargetSize,
		VertexShader,
		EDRF_UseTriangleOptimization);
}

bool FMobileSceneRenderer::ShouldRenderHZB()
{
	static const auto MobileAmbientOcclusionTechniqueCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AmbientOcclusionTechnique"));

	// Mobile SSAO requests HZB
	bool bIsFeatureRequested = bRequiresAmbientOcclusionPass && MobileAmbientOcclusionTechniqueCVar->GetValueOnRenderThread() == 1;

	// Instance occlusion culling requires HZB
	if (FInstanceCullingContext::IsOcclusionCullingEnabled())
	{
		bIsFeatureRequested = true;
	}

	bool bNeedsHZB = bIsFeatureRequested;

	return bNeedsHZB;
}

void FMobileSceneRenderer::RenderHZB(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& SceneDepthZ)
{
	checkSlow(bShouldRenderHZB);

	FRDGBuilder GraphBuilder(RHICmdList);
	{
		FRDGTextureRef SceneDepthTexture = GraphBuilder.RegisterExternalTexture(SceneDepthZ, TEXT("SceneDepthTexture"));

		RenderHZB(GraphBuilder, SceneDepthTexture);
	}
	GraphBuilder.Execute();
}

void FMobileSceneRenderer::RenderHZB(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneDepthTexture)
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, HZB);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		if (View.ShouldRenderView())
		{
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
			{
				RDG_EVENT_SCOPE(GraphBuilder, "BuildHZB(ViewId=%d)", ViewIndex);

				FRDGTextureRef FurthestHZBTexture = nullptr;

				BuildHZBFurthest(
					GraphBuilder,
					SceneDepthTexture,
					/* VisBufferTexture = */ nullptr,
					View.ViewRect,
					View.GetFeatureLevel(),
					View.GetShaderPlatform(),
					TEXT("MobileHZBFurthest"),
					&FurthestHZBTexture);

				View.HZBMipmap0Size = FurthestHZBTexture->Desc.Extent;
				View.HZB = FurthestHZBTexture;

				if (View.ViewState)
				{
					if (FInstanceCullingContext::IsOcclusionCullingEnabled())
					{
						GraphBuilder.QueueTextureExtraction(FurthestHZBTexture, &View.ViewState->PrevFrameViewInfo.HZB);
					}
					else
					{
						View.ViewState->PrevFrameViewInfo.HZB = nullptr;
					}
				}
			}

			if (Scene->InstanceCullingOcclusionQueryRenderer && View.ViewState)
			{
				// Render per-instance occlusion queries and save the mask to interpret results on the next frame
				const uint32 OcclusionQueryMaskForThisView = Scene->InstanceCullingOcclusionQueryRenderer->Render(GraphBuilder, Scene->GPUScene, View);
				View.ViewState->PrevFrameViewInfo.InstanceOcclusionQueryMask = OcclusionQueryMaskForThisView;
			}
		}
	}
}

bool FMobileSceneRenderer::AllowSimpleLights() const
{
	return FSceneRenderer::AllowSimpleLights() && bDeferredShading;
}
