// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InstanceCulling/InstanceCullingContext.h"
#include "InstanceCullingLoadBalancer.h"
#include "Nanite/Nanite.h"
#include "RHI.h"
#include "RenderGraphResources.h"
#include "SceneManagement.h"

class FGPUScene;

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

struct FInstanceCullingResult
{
	//TRefCountPtr<FRDGPooledBuffer> InstanceIdsBuffer;
	FRDGBufferRef DrawIndirectArgsBuffer = nullptr;
	FRDGBufferRef InstanceDataBuffer = nullptr;
	// Offset for both buffers to start fetching data at, used when batching multiple culling jobs in the same buffer
	uint32 InstanceDataByteOffset = 0U;
	uint32 IndirectArgsByteOffset = 0U;
	TRDGUniformBufferRef<FInstanceCullingGlobalUniforms> UniformBuffer = nullptr;

	//FRHIBuffer* GetDrawIndirectArgsBufferRHI() const { return DrawIndirectArgsBuffer.IsValid() ? DrawIndirectArgsBuffer->GetVertexBufferRHI() : nullptr; }
	//FRHIBuffer* GetInstanceIdOffsetBufferRHI() const { return InstanceIdOffsetBuffer.IsValid() ? InstanceIdOffsetBuffer->GetVertexBufferRHI() : nullptr; }
	void GetDrawParameters(FInstanceCullingDrawParams &OutParams) const
	{
		// GPUCULL_TODO: Maybe get dummy buffers?
		OutParams.DrawIndirectArgsBuffer = DrawIndirectArgsBuffer;
		OutParams.InstanceIdOffsetBuffer = InstanceDataBuffer;
		OutParams.InstanceDataByteOffset = InstanceDataByteOffset;
		OutParams.IndirectArgsByteOffset = IndirectArgsByteOffset;
		OutParams.InstanceCulling = UniformBuffer;
	}

	static void CondGetDrawParameters(const FInstanceCullingResult* InstanceCullingResult, FInstanceCullingDrawParams& OutParams)
	{
		if (InstanceCullingResult)
		{
			InstanceCullingResult->GetDrawParameters(OutParams);
		}
		else
		{
			OutParams.DrawIndirectArgsBuffer = nullptr;
			OutParams.InstanceIdOffsetBuffer = nullptr;
			OutParams.InstanceDataByteOffset = 0U;
			OutParams.IndirectArgsByteOffset = 0U;
			OutParams.InstanceCulling = nullptr;
		}
	}
};

class FInstanceCullingDeferredContext;

/**
 * Manages allocation of indirect arguments and culling jobs for all instanced draws (use the GPU Scene culling).
 */
class FInstanceCullingManager
{
public:
	FInstanceCullingManager(bool bInIsEnabled, FRDGBuilder& GraphBuilder);
	~FInstanceCullingManager();

	bool IsEnabled() const { return bIsEnabled; }

	// Register a view for culling, returns integer ID of the view.
	int32 RegisterView(const Nanite::FPackedViewParams& Params);
	
	// Helper to translate from view info, extracts the needed data for setting up instance culling.
	int32 RegisterView(const FViewInfo& ViewInfo);

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

	const TArray<Nanite::FPackedView>& GetCullingViews() { return CullingViews; }

private:

	friend class FInstanceCullingContext;
	
	// Polulated by FInstanceCullingContext::BuildRenderingCommandsDeferred, used to hold instance culling related data that needs to be passed around
	FInstanceCullingDeferredContext *DeferredContext = nullptr;

	FInstanceCullingManager() = delete;
	FInstanceCullingManager(FInstanceCullingManager &) = delete;

	TArray<Nanite::FPackedView> CullingViews;
	bool bIsEnabled;
};

