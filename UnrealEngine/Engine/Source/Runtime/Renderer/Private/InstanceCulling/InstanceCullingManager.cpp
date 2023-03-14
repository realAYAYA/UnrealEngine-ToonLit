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

FInstanceCullingManager::FInstanceCullingManager(bool bInIsEnabled, FRDGBuilder& GraphBuilder)
	: bIsEnabled(bInIsEnabled)
{
	CullingIntermediate.DummyUniformBuffer = FInstanceCullingContext::CreateDummyInstanceCullingUniformBuffer(GraphBuilder);
}

FInstanceCullingManager::~FInstanceCullingManager()
{
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
	Params.HZBTestViewRect = FIntRect(0, 0, ViewInfo.PrevViewInfo.ViewRect.Width(), ViewInfo.PrevViewInfo.ViewRect.Height());	// needs to be in HZB space, which is 0,0-based for any view, even stereo/splitscreen ones
	return RegisterView(Params);
}



int32 FInstanceCullingManager::RegisterView(const Nanite::FPackedViewParams& Params)
{
	if (!bIsEnabled)
	{
		return 0;
	}
	CullingViews.Add(CreatePackedView(Params));
	return CullingViews.Num() - 1;
}

void FInstanceCullingManager::FlushRegisteredViews(FRDGBuilder& GraphBuilder)
{
	if (CullingIntermediate.NumViews != CullingViews.Num())
	{
		CullingIntermediate.CullingViews = CreateStructuredBuffer(GraphBuilder, TEXT("InstanceCulling.CullingViews"), CullingViews);
		CullingIntermediate.NumViews = CullingViews.Num();
	}
}

bool FInstanceCullingManager::AllowBatchedBuildRenderingCommands(const FGPUScene& GPUScene)
{
	return GPUScene.IsEnabled() && !!GAllowBatchedBuildRenderingCommands && !FRDGBuilder::IsImmediateMode();
}

void FInstanceCullingManager::BeginDeferredCulling(FRDGBuilder& GraphBuilder, FGPUScene& GPUScene)
{
	FlushRegisteredViews(GraphBuilder);

	// Cannot defer pass execution in immediate mode.
	if (!AllowBatchedBuildRenderingCommands(GPUScene))
	{
		return;
	}

	// If there are no instances, there can be no work to perform later.
	if (GPUScene.GetNumInstances() == 0 || CullingViews.Num() == 0)
	{
		return;
	}

	DeferredContext = FInstanceCullingContext::CreateDeferredContext(GraphBuilder, GPUScene, this);
}
