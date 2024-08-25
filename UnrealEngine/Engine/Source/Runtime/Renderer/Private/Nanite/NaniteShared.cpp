// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteShared.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "Rendering/NaniteStreamingManager.h"
#include "SceneRelativeViewMatrices.h"
#include "UnrealEngine.h"

DEFINE_LOG_CATEGORY(LogNanite);
DEFINE_GPU_STAT(NaniteDebug);

IMPLEMENT_STATIC_UNIFORM_BUFFER_SLOT(Nanite);
IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FNaniteUniformParameters, "Nanite", Nanite);

IMPLEMENT_STATIC_UNIFORM_BUFFER_SLOT(NaniteRayTracing);
IMPLEMENT_STATIC_AND_SHADER_UNIFORM_BUFFER_STRUCT(FNaniteRayTracingUniformParameters, "NaniteRayTracing", NaniteRayTracing);

extern TAutoConsoleVariable<float> CVarNaniteMaxPixelsPerEdge;
extern TAutoConsoleVariable<float> CVarNaniteMinPixelsPerEdgeHW;

// Optimized compute dual depth export pass on supported platforms.
int32 GNaniteExportDepth = 1;
static FAutoConsoleVariableRef CVarNaniteExportDepth(
	TEXT("r.Nanite.ExportDepth"),
	GNaniteExportDepth,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GNaniteMaxNodes = 2 * 1048576;
FAutoConsoleVariableRef CVarNaniteMaxNodes(
	TEXT("r.Nanite.MaxNodes"),
	GNaniteMaxNodes,
	TEXT("Maximum number of Nanite nodes traversed during a culling pass."),
	ECVF_RenderThreadSafe
);

int32 GNaniteMaxCandidateClusters = 16 * 1048576;
FAutoConsoleVariableRef CVarNaniteMaxCandidateClusters(
	TEXT("r.Nanite.MaxCandidateClusters"),
	GNaniteMaxCandidateClusters,
	TEXT("Maximum number of Nanite clusters before cluster culling."),
	ECVF_RenderThreadSafe
);

int32 GNaniteMaxVisibleClusters = 4 * 1048576;
FAutoConsoleVariableRef CVarNaniteMaxVisibleClusters(
	TEXT("r.Nanite.MaxVisibleClusters"),
	GNaniteMaxVisibleClusters,
	TEXT("Maximum number of visible Nanite clusters."),
	ECVF_RenderThreadSafe
);

int32 GNaniteMaxCandidatePatches = 2 * 1048576;
FAutoConsoleVariableRef CVarNaniteMaxCandidatePatches(
	TEXT("r.Nanite.MaxCandidatePatches"),
	GNaniteMaxCandidatePatches,
	TEXT("Maximum number of Nanite patches considered for splitting."),
	ECVF_RenderThreadSafe
);

int32 GNaniteMaxVisiblePatches = 2 * 1048576;
FAutoConsoleVariableRef CVarNaniteMaxVisiblePatches(
	TEXT("r.Nanite.MaxVisiblePatches"),
	GNaniteMaxVisiblePatches,
	TEXT("Maximum number of visible Nanite patches."),
	ECVF_RenderThreadSafe
);

#define MAX_CLUSTERS	(16 * 1024 * 1024)


namespace Nanite
{

void FPackedView::UpdateLODScales(const float NaniteMaxPixelsPerEdge, const float MinPixelsPerEdgeHW)
{
	const float ViewToPixels = 0.5f * ViewToClip.M[1][1] * ViewSizeAndInvSize.Y;

	const float LODScale = ViewToPixels / NaniteMaxPixelsPerEdge;
	const float LODScaleHW = ViewToPixels / MinPixelsPerEdgeHW;

	LODScales = FVector2f(LODScale, LODScaleHW);
}

void SetCullingViewOverrides(FViewInfo const* InCullingView, Nanite::FPackedViewParams& InOutParams)
{
	if (InCullingView != nullptr)
	{
		// Culling uses main view for distance and screen size.
		InOutParams.bUseCullingViewOverrides = true;
		InOutParams.CullingViewOrigin = InCullingView->ViewMatrices.GetViewOrigin();
		InOutParams.CullingViewScreenMultiple = FMath::Max(InCullingView->ViewMatrices.GetProjectionMatrix().M[0][0], InCullingView->ViewMatrices.GetProjectionMatrix().M[1][1]);
		// We bake the view lod scales into ScreenMultiple since the two things are always used together.
		const float LODDistanceScale = GetCachedScalabilityCVars().StaticMeshLODDistanceScale * InCullingView->LODDistanceFactor;
		InOutParams.CullingViewScreenMultiple /= LODDistanceScale;
	}
	else
	{
		InOutParams.bUseCullingViewOverrides = false;
	}
}

FPackedView CreatePackedView( const FPackedViewParams& Params )
{
	// NOTE: There is some overlap with the logic - and this should stay consistent with - FSceneView::SetupViewRectUniformBufferParameters
	// Longer term it would be great to refactor a common place for both of this logic, but currently FSceneView has a lot of heavy-weight
	// stuff in it beyond the relevant parameters to SetupViewRectUniformBufferParameters (and Nanite has a few of its own parameters too).

	const FDFRelativeViewMatrices RelativeMatrices = FDFRelativeViewMatrices::Create(Params.ViewMatrices, Params.PrevViewMatrices);
	const FDFVector3 AbsoluteViewOrigin(Params.ViewMatrices.GetViewOrigin());
	const FVector ViewHigh(AbsoluteViewOrigin.High);
	const FDFVector3 AbsolutePreViewTranslation(Params.ViewMatrices.GetPreViewTranslation()); // Usually equal to -AbsoluteViewOrigin, but there are some ortho edge cases

	const FIntRect& ViewRect = Params.ViewRect;
	const FVector4f ViewSizeAndInvSize(ViewRect.Width(), ViewRect.Height(), 1.0f / float(ViewRect.Width()), 1.0f / float(ViewRect.Height()));

	const float NaniteMaxPixelsPerEdge = CVarNaniteMaxPixelsPerEdge.GetValueOnRenderThread() * Params.MaxPixelsPerEdgeMultipler;
	const float NaniteMinPixelsPerEdgeHW = CVarNaniteMinPixelsPerEdgeHW.GetValueOnRenderThread();
	
	const FVector CullingViewOrigin = Params.bUseCullingViewOverrides ? Params.CullingViewOrigin : Params.ViewMatrices.GetViewOrigin();
	// We bake the view lod scales into ScreenMultiple since the two things are always used together.
	const float ViewDistanceLODScale = GetCachedScalabilityCVars().StaticMeshLODDistanceScale * Params.ViewLODDistanceFactor;
	const float ScreenMultiple = FMath::Max(Params.ViewMatrices.GetProjectionMatrix().M[0][0], Params.ViewMatrices.GetProjectionMatrix().M[1][1]) / ViewDistanceLODScale;
	const float CullingViewScreenMulitple = Params.bUseCullingViewOverrides && Params.CullingViewScreenMultiple > 0.f ? Params.CullingViewScreenMultiple : ScreenMultiple;

	const FDFVector3 PrevPreViewTranslation(Params.PrevViewMatrices.GetPreViewTranslation());

	FPackedView PackedView;
	PackedView.TranslatedWorldToView		= FMatrix44f(Params.ViewMatrices.GetOverriddenTranslatedViewMatrix());	// LWC_TODO: Precision loss? (and below)
	PackedView.TranslatedWorldToClip		= FMatrix44f(Params.ViewMatrices.GetTranslatedViewProjectionMatrix());
	PackedView.ViewToClip					= RelativeMatrices.ViewToClip;
	PackedView.ClipToRelativeWorld			= RelativeMatrices.ClipToRelativeWorld;
	PackedView.PreViewTranslationHigh		= AbsolutePreViewTranslation.High;
	PackedView.PreViewTranslationLow		= AbsolutePreViewTranslation.Low;
	PackedView.ViewOriginLow				= AbsoluteViewOrigin.Low;
	PackedView.CullingViewOriginTranslatedWorld = FVector3f(CullingViewOrigin + Params.ViewMatrices.GetPreViewTranslation());
	PackedView.ViewForward					= (FVector3f)Params.ViewMatrices.GetOverriddenTranslatedViewMatrix().GetColumn(2);
	PackedView.NearPlane					= Params.ViewMatrices.ComputeNearPlane();
	PackedView.ViewOriginHighX				= AbsoluteViewOrigin.High.X;
	PackedView.ViewOriginHighY				= AbsoluteViewOrigin.High.Y;
	PackedView.ViewOriginHighZ				= AbsoluteViewOrigin.High.Z;
	PackedView.RangeBasedCullingDistance	= Params.RangeBasedCullingDistance;
	PackedView.CullingViewScreenMultiple	= CullingViewScreenMulitple;

	PackedView.PrevTranslatedWorldToView	= FMatrix44f(Params.PrevViewMatrices.GetOverriddenTranslatedViewMatrix()); // LWC_TODO: Precision loss? (and below)
	PackedView.PrevTranslatedWorldToClip	= FMatrix44f(Params.PrevViewMatrices.GetTranslatedViewProjectionMatrix());
	PackedView.PrevViewToClip				= FMatrix44f(Params.PrevViewMatrices.GetProjectionMatrix());
	PackedView.PrevClipToRelativeWorld		= RelativeMatrices.PrevClipToRelativeWorld;
	PackedView.PrevPreViewTranslationHigh	= PrevPreViewTranslation.High;
	PackedView.PrevPreViewTranslationLow	= PrevPreViewTranslation.Low;

	PackedView.ViewRect = FIntVector4(ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X, ViewRect.Max.Y);
	PackedView.ViewSizeAndInvSize = ViewSizeAndInvSize;

	// Transform clip from full screen to viewport.
	FVector2D RcpRasterContextSize = FVector2D(1.0f / Params.RasterContextSize.X, 1.0f / Params.RasterContextSize.Y);
	PackedView.ClipSpaceScaleOffset = FVector4f(ViewSizeAndInvSize.X * RcpRasterContextSize.X,
		ViewSizeAndInvSize.Y * RcpRasterContextSize.Y,
		(ViewSizeAndInvSize.X + 2.0f * ViewRect.Min.X) * RcpRasterContextSize.X - 1.0f,
		-(ViewSizeAndInvSize.Y + 2.0f * ViewRect.Min.Y) * RcpRasterContextSize.Y + 1.0f);

	const float Mx = 2.0f * ViewSizeAndInvSize.Z;
	const float My = -2.0f * ViewSizeAndInvSize.W;
	const float Ax = -1.0f - 2.0f * ViewRect.Min.X * ViewSizeAndInvSize.Z;
	const float Ay = 1.0f + 2.0f * ViewRect.Min.Y * ViewSizeAndInvSize.W;

	PackedView.SVPositionToTranslatedWorld = FMatrix44f(			// LWC_TODO: Precision loss? (and below)
		FMatrix(FPlane(Mx, 0, 0, 0),
			FPlane(0, My, 0, 0),
			FPlane(0, 0, 1, 0),
			FPlane(Ax, Ay, 0, 1)) * Params.ViewMatrices.GetInvTranslatedViewProjectionMatrix());
	PackedView.ViewToTranslatedWorld = FMatrix44f(Params.ViewMatrices.GetOverriddenInvTranslatedViewMatrix());	

	check(Params.StreamingPriorityCategory <= NANITE_STREAMING_PRIORITY_CATEGORY_MASK);
	PackedView.StreamingPriorityCategory_AndFlags = (Params.Flags << NANITE_NUM_STREAMING_PRIORITY_CATEGORY_BITS) | Params.StreamingPriorityCategory;
	PackedView.MinBoundsRadiusSq = Params.MinBoundsRadius * Params.MinBoundsRadius;
	PackedView.UpdateLODScales(NaniteMaxPixelsPerEdge, NaniteMinPixelsPerEdgeHW);

	PackedView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.X = Params.TargetLayerIndex;
	PackedView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Y = Params.TargetMipLevel;
	PackedView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Z = Params.TargetMipCount;
	PackedView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.W = Params.PrevTargetLayerIndex;

	PackedView.HZBTestViewRect = FIntVector4(Params.HZBTestViewRect.Min.X, Params.HZBTestViewRect.Min.Y, Params.HZBTestViewRect.Max.X, Params.HZBTestViewRect.Max.Y);

	FPlane TranslatedPlane(Params.GlobalClippingPlane.TranslateBy(Params.ViewMatrices.GetPreViewTranslation()));
	PackedView.TranslatedGlobalClipPlane = FVector4f(TranslatedPlane.X, TranslatedPlane.Y, TranslatedPlane.Z, -TranslatedPlane.W);
	
	PackedView.InstanceOcclusionQueryMask = Params.InstanceOcclusionQueryMask;
	PackedView.LightingChannelMask = Params.LightingChannelMask;
	
	return PackedView;

}

FPackedViewArray* FPackedViewArray::Create(FRDGBuilder& GraphBuilder, const FPackedView& View)
{
	FPackedViewArray* ViewArray = GraphBuilder.AllocObject<FPackedViewArray>(1, 1);
	ViewArray->Views.Add(View);
	return ViewArray;
}

FPackedViewArray* FPackedViewArray::Create(FRDGBuilder& GraphBuilder, uint32 NumPrimaryViews, uint32 MaxNumMips, ArrayType&& View)
{
	FPackedViewArray* ViewArray = GraphBuilder.AllocObject<FPackedViewArray>(NumPrimaryViews, MaxNumMips);
	ViewArray->Views = Forward<ArrayType&&>(View);
	checkf(ViewArray->Views.Num() == ViewArray->NumViews, TEXT("Expected View array to have %d elements, but it only has %d"), ViewArray->Views.Num(), ViewArray->NumViews);
	return ViewArray;
}

FPackedViewArray* FPackedViewArray::CreateWithSetupTask(FRDGBuilder& GraphBuilder, uint32 NumPrimaryViews, uint32 MaxNumMips, TaskLambdaType&& TaskLambda, UE::Tasks::FPipe* Pipe, bool bExecuteInTask)
{
	FPackedViewArray* ViewArray = GraphBuilder.AllocObject<FPackedViewArray>(NumPrimaryViews, MaxNumMips);

	ViewArray->SetupTask = GraphBuilder.AddSetupTask([ViewArray, TaskLambda = MoveTemp(TaskLambda)]
	{
		ViewArray->Views.Reserve(ViewArray->NumViews);
		TaskLambda(ViewArray->Views);
		checkf(ViewArray->Views.Num() == ViewArray->NumViews, TEXT("Expected View array to have %d elements, but it only has %d"), ViewArray->Views.Num(), ViewArray->NumViews);

	}, Pipe, UE::Tasks::ETaskPriority::Normal, bExecuteInTask);

	return ViewArray;
}

FPackedView CreatePackedViewFromViewInfo
(
	const FViewInfo& View,
	FIntPoint RasterContextSize,
	uint32 Flags,
	uint32 StreamingPriorityCategory,
	float MinBoundsRadius,
	float MaxPixelsPerEdgeMultipler,
	const FIntRect* InHZBTestViewRect
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CreatePackedViewFromViewInfo);
	FPackedViewParams Params;
	Params.ViewMatrices = View.ViewMatrices;
	Params.PrevViewMatrices = View.PrevViewInfo.ViewMatrices;
	Params.ViewRect = View.ViewRect;
	Params.RasterContextSize = RasterContextSize;
	Params.Flags = Flags | (View.bReverseCulling ? NANITE_VIEW_FLAG_REVERSE_CULLING : 0);
	Params.StreamingPriorityCategory = StreamingPriorityCategory;
	Params.MinBoundsRadius = MinBoundsRadius;
	Params.ViewLODDistanceFactor = View.LODDistanceFactor;
	// Note - it is incorrect to use ViewRect as it is in a different space, but keeping this for backward compatibility reasons with other callers
	Params.HZBTestViewRect = InHZBTestViewRect ? *InHZBTestViewRect : View.PrevViewInfo.ViewRect;
	Params.MaxPixelsPerEdgeMultipler = MaxPixelsPerEdgeMultipler;
	Params.GlobalClippingPlane = View.GlobalClippingPlane;
	return CreatePackedView(Params);
}

bool ShouldDrawSceneViewsInOneNanitePass(const FViewInfo& View)
{
	static const TConsoleVariableData<int32>* CVarDrawSceneViewsInOneNanitePass = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Nanite.MultipleSceneViewsInOnePass"));
	return View.bIsMultiViewportEnabled && CVarDrawSceneViewsInOneNanitePass && (CVarDrawSceneViewsInOneNanitePass->GetValueOnRenderThread() > 0);
}

void FGlobalResources::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		LLM_SCOPE_BYTAG(Nanite);
#if !UE_BUILD_SHIPPING
		FeedbackManager = new FFeedbackManager();
#endif
		PickingBuffers.AddZeroed(MaxPickingBuffers);
	}
}

void FGlobalResources::ReleaseRHI()
{
	if (DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		LLM_SCOPE_BYTAG(Nanite);

		PickingBuffers.Reset();

		SplitWorkQueueBuffer.SafeRelease();
		OccludedPatchesBuffer.SafeRelease();

		MainPassBuffers.StatsRasterizeArgsSWHWBuffer.SafeRelease();
		PostPassBuffers.StatsRasterizeArgsSWHWBuffer.SafeRelease();

		MainAndPostNodesAndClusterBatchesBuffer.Buffer.SafeRelease();

		StatsBuffer.SafeRelease();
		ShadingBinDataBuffer.SafeRelease();
		FastClearTileVis.SafeRelease();

#if !UE_BUILD_SHIPPING
		delete FeedbackManager;
		FeedbackManager = nullptr;
#endif
	}
}

void FGlobalResources::Update(FRDGBuilder& GraphBuilder)
{
	check(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));
}

uint32 FGlobalResources::GetMaxCandidateClusters()
{
	// NOTE: Candidate clusters can currently be allowed to exceed MAX_CLUSTERS
	const uint32 MaxCandidateClusters = GNaniteMaxCandidateClusters & -NANITE_PERSISTENT_CLUSTER_CULLING_GROUP_SIZE;
	return MaxCandidateClusters;
}

uint32 FGlobalResources::GetMaxClusterBatches()
{
	const uint32 MaxCandidateClusters = GetMaxCandidateClusters();
	check(MaxCandidateClusters % NANITE_PERSISTENT_CLUSTER_CULLING_GROUP_SIZE == 0);
	return MaxCandidateClusters / NANITE_PERSISTENT_CLUSTER_CULLING_GROUP_SIZE;
}

uint32 FGlobalResources::GetMaxVisibleClusters()
{
	checkf(GNaniteMaxVisibleClusters <= MAX_CLUSTERS, TEXT("r.Nanite.MaxVisibleClusters must be <= MAX_CLUSTERS"));
	return GNaniteMaxVisibleClusters;
}

uint32 FGlobalResources::GetMaxNodes()
{
	return GNaniteMaxNodes & -NANITE_MAX_BVH_NODES_PER_GROUP;
}

uint32 FGlobalResources::GetMaxCandidatePatches()
{
	return GNaniteMaxCandidatePatches;
}

uint32 FGlobalResources::GetMaxVisiblePatches()
{
	return GNaniteMaxVisiblePatches;
}

TGlobalResource< FGlobalResources > GGlobalResources;

} // namespace Nanite

bool ShouldRenderNanite(const FScene* Scene, const FViewInfo& View, bool bCheckForAtomicSupport)
{
	// Does the platform support Nanite (with 64bit image atomics), and is it enabled?
	if (Scene && UseNanite(Scene->GetShaderPlatform(), bCheckForAtomicSupport))
	{
		// Any resources registered to the streaming manager?
		if (Nanite::GStreamingManager.HasResourceEntries())
		{
			// Is the view family showing Nanite meshes?
			return View.Family->EngineShowFlags.NaniteMeshes;
		}
	}

	// Nanite should not render for this view
	return false;
}

bool WouldRenderNanite(const FScene* Scene, const FViewInfo& View, bool bCheckForAtomicSupport, bool bCheckForProjectSetting)
{
	// Does the platform support Nanite (with 64bit image atomics), and is it enabled?
	if (Scene && UseNanite(Scene->GetShaderPlatform(), bCheckForAtomicSupport, bCheckForProjectSetting))
	{
		// Is the view family showing would-be Nanite meshes?
		return View.Family->EngineShowFlags.NaniteMeshes;
	}

	// Nanite would not render for this view
	return false;
}


bool UseComputeDepthExport()
{
	return (GRHISupportsDepthUAV && GRHISupportsExplicitHTile && GNaniteExportDepth != 0);
}
