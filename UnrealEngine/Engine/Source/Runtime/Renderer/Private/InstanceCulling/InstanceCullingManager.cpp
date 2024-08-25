// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceCullingManager.h"
#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererModule.h"
#include "ShaderParameterMacros.h"
#include "RenderGraphResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "InstanceCulling/InstanceCullingContext.h"

static int32 GAllowBatchedBuildRenderingCommands = 1;
static FAutoConsoleVariableRef CVarAllowBatchedBuildRenderingCommands(
	TEXT("r.InstanceCulling.AllowBatchedBuildRenderingCommands"),
	GAllowBatchedBuildRenderingCommands,
	TEXT("Whether to allow batching BuildRenderingCommands for GPU instance culling"),
	ECVF_RenderThreadSafe);

FInstanceCullingManager::FInstanceCullingManager(FSceneUniformBuffer& SceneUB, bool bInIsEnabled, FRDGBuilder& GraphBuilder)
	: SceneUB(SceneUB), bIsEnabled(bInIsEnabled)
{
	CullingIntermediate.DummyUniformBuffer = FInstanceCullingContext::CreateDummyInstanceCullingUniformBuffer(GraphBuilder);
}

FInstanceCullingManager::~FInstanceCullingManager()
{
}

void FInstanceCullingManager::AllocateViews(int32 NumViews)
{
	if (bIsEnabled)
	{
		CullingViews.AddUninitialized(CullingViews.Num() + NumViews);
	}
}

int32 FInstanceCullingManager::RegisterView(const FViewInfo& ViewInfo)
{
	if (!bIsEnabled)
	{
		return 0;
	}

	Nanite::FPackedViewParams Params;
	Params.ViewMatrices = ViewInfo.ViewMatrices;
	Params.PrevViewMatrices = ViewInfo.PrevViewInfo.ViewMatrices;
	Params.ViewRect = ViewInfo.ViewRect;
	// TODO: faking this here (not needed for culling, until we start involving multi-view and HZB)
	Params.RasterContextSize = ViewInfo.ViewRect.Size();
	Params.ViewLODDistanceFactor = ViewInfo.LODDistanceFactor;
	Params.HZBTestViewRect = FIntRect(0, 0, ViewInfo.PrevViewInfo.ViewRect.Width(), ViewInfo.PrevViewInfo.ViewRect.Height());	// needs to be in HZB space, which is 0,0-based for any view, even stereo/splitscreen ones
	Params.MaxPixelsPerEdgeMultipler = 1.0f;
	Params.InstanceOcclusionQueryMask = ViewInfo.PrevViewInfo.InstanceOcclusionQueryMask;

	return RegisterView(Params);
}



int32 FInstanceCullingManager::RegisterView(const Nanite::FPackedViewParams& Params)
{
	if (!bIsEnabled)
	{
		return 0;
	}

	const int32 ViewIndex = NumRegisteredViews.fetch_add(1, std::memory_order_relaxed);
	check(ViewIndex <= CullingViews.Num());
	CullingViews[ViewIndex] = CreatePackedView(Params);
	return ViewIndex;
}

void FInstanceCullingManager::FlushRegisteredViews(FRDGBuilder& GraphBuilder)
{
	const int32 LocalNumRegisteredViews = NumRegisteredViews.load(std::memory_order_relaxed);

	if (CullingIntermediate.NumViews != LocalNumRegisteredViews)
	{
		CullingIntermediate.CullingViews = CreateStructuredBuffer(GraphBuilder, TEXT("InstanceCulling.CullingViews"), MakeArrayView<const Nanite::FPackedView>(CullingViews.GetData(), LocalNumRegisteredViews));
		CullingIntermediate.NumViews = LocalNumRegisteredViews;
	}
}

bool FInstanceCullingManager::AllowBatchedBuildRenderingCommands(const FGPUScene& GPUScene)
{
	return GPUScene.IsEnabled() && !!GAllowBatchedBuildRenderingCommands && !FRDGBuilder::IsImmediateMode();
}

void FInstanceCullingManager::BeginDeferredCulling(FRDGBuilder& GraphBuilder, FGPUScene& GPUScene)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FInstanceCullingManager::BeginDeferredCulling);
	FlushRegisteredViews(GraphBuilder);

	// Cannot defer pass execution in immediate mode.
	if (!AllowBatchedBuildRenderingCommands(GPUScene))
	{
		return;
	}

	// If there are no instances, there can be no work to perform later.
	if (GPUScene.GetNumInstances() == 0 || NumRegisteredViews.load(std::memory_order_relaxed) == 0)
	{
		return;
	}

	DeferredContext = FInstanceCullingContext::CreateDeferredContext(GraphBuilder, GPUScene, *this);
}
