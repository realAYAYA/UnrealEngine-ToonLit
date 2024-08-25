// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstanceCulling/InstanceCullingContext.h"
#include "InstanceCullingLoadBalancer.h"
#include "RenderGraphResources.h"
#include "Async/Mutex.h"

class FGPUScene;
class FSceneUniformBuffer;

namespace Nanite
{
	struct FPackedView;
	struct FPackedViewParams;
}

class FInstanceProcessingGPULoadBalancer : public TInstanceCullingLoadBalancer<>
{
public:
};


class FInstanceCullingIntermediate
{
public:
	/**
	 * All registered views that may be used for culling.
	 */
	FRDGBufferRef CullingViews = nullptr;
	int32 NumViews = 0;

	TRDGUniformBufferRef<FInstanceCullingGlobalUniforms> DummyUniformBuffer;
};

/**
 * Only needed for compatibility, used to explicitly opt out of async processing (when there is no capturable pointer to an FInstanceCullingDrawParams).
 */
struct FInstanceCullingResult
{
	FInstanceCullingDrawParams Parameters;

	inline void GetDrawParameters(FInstanceCullingDrawParams &OutParams) const
	{
		OutParams = Parameters;
	}
};

class FInstanceCullingDeferredContext;

/**
 * Manages allocation of indirect arguments and culling jobs for all instanced draws (use the GPU Scene culling).
 */
class FInstanceCullingManager
{
public:
	FInstanceCullingManager(FSceneUniformBuffer& SceneUB, bool bInIsEnabled, FRDGBuilder& GraphBuilder);
	~FInstanceCullingManager();

	bool IsEnabled() const { return bIsEnabled; }

	// Register a view for culling, returns integer ID of the view.
	int32 RegisterView(const Nanite::FPackedViewParams& Params);
	
	// Helper to translate from view info, extracts the needed data for setting up instance culling.
	int32 RegisterView(const FViewInfo& ViewInfo);

	// Allocate space for views ahead of time prior to calling RegisterView.
	void AllocateViews(int32 NumViews);

	/**
	 * Upload all registered views if the number has changed (grown). This must be done before calls to FInstanceCullingContext::BuildRenderingCommands that references
	 * the views.
	 */
	void FlushRegisteredViews(FRDGBuilder& GraphBuilder);

	const TRDGUniformBufferRef<FInstanceCullingGlobalUniforms> GetDummyInstanceCullingUniformBuffer() const { return CullingIntermediate.DummyUniformBuffer; }
	
	static bool AllowBatchedBuildRenderingCommands(const FGPUScene& GPUScene);


	/**
	 * Add a deferred, batched, gpu culling pass. Each batch represents a BuildRenderingCommands call from a mesh pass.
	 * Batches are collected as we walk through the main render setup and call BuildRenderingCommands, and are processed when RDG Execute or Drain is called.
	 * This implicitly ends the deferred context, so if Drain is used, it should be paired with a new call to BeginDeferredCulling.
	 * Can be called multiple times, and will collect subsequent BuildRenderingCommands. Care must be taken that the views referenced in the build rendering commands
	 * have been registered before BeginDeferredCulling.
	 * Calls FlushRegisteredViews that uploads the registered views to the GPU.
	 */
	void BeginDeferredCulling(FRDGBuilder& GraphBuilder, FGPUScene& GPUScene);


	/** Whether we are actively batching GPU instance culling work. */
	bool IsDeferredCullingActive() const { return DeferredContext != nullptr; }

	// Populated by CullInstances, used when performing final culling & rendering 
	FInstanceCullingIntermediate CullingIntermediate;

	// Reference to a buffer owned by FInstanceCullingOcclusionQueryRenderer
	FRDGBufferRef InstanceOcclusionQueryBuffer = {};
	EPixelFormat InstanceOcclusionQueryBufferFormat = PF_Unknown;

private:

	friend class FInstanceCullingContext;
	
	// Polulated by FInstanceCullingContext::BuildRenderingCommandsDeferred, used to hold instance culling related data that needs to be passed around
	FInstanceCullingDeferredContext *DeferredContext = nullptr;

	FInstanceCullingManager() = delete;
	FInstanceCullingManager(FInstanceCullingManager &) = delete;

	FSceneUniformBuffer& SceneUB;
	std::atomic_int32_t NumRegisteredViews = {0};
	TArray<Nanite::FPackedView> CullingViews;
	bool bIsEnabled;
};

