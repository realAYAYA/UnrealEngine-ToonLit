// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIValidationContext.h: Public RHI Validation Context definitions.
=============================================================================*/

#pragma once

#include "RHIValidationCommon.h"
#include "RHIValidationUtils.h"
#include "RHIValidation.h"

#if ENABLE_RHI_VALIDATION

#include "RHI.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "RHIUtilities.h"
#endif

class FValidationRHI;

inline void ValidateShaderParameters(FRHIShader* RHIShader, RHIValidation::FTracker* Tracker, RHIValidation::FStaticUniformBuffers& StaticUniformBuffers, TConstArrayView<FRHIShaderParameterResource> InParameters, ERHIAccess InRequiredAccess, RHIValidation::EUAVMode InRequiredUAVMode)
{
	for (const FRHIShaderParameterResource& Parameter : InParameters)
	{
		switch (Parameter.Type)
		{
		case FRHIShaderParameterResource::EType::Texture:
			if (FRHITexture* Texture = static_cast<FRHITexture*>(Parameter.Resource))
			{
				if (GRHIValidationEnabled)
				{
					RHIValidation::ValidateShaderResourceView(RHIShader, Parameter.Index, Texture);
				}
				Tracker->Assert(Texture->GetWholeResourceIdentitySRV(), InRequiredAccess);
			}
			break;
		case FRHIShaderParameterResource::EType::ResourceView:
			if (FRHIShaderResourceView* SRV = static_cast<FRHIShaderResourceView*>(Parameter.Resource))
			{
				if (GRHIValidationEnabled)
				{
					RHIValidation::ValidateShaderResourceView(RHIShader, Parameter.Index, SRV);
				}
				Tracker->Assert(SRV->GetViewIdentity(), InRequiredAccess);
			}
			break;
		case FRHIShaderParameterResource::EType::UnorderedAccessView:
			if (FRHIUnorderedAccessView* UAV = static_cast<FRHIUnorderedAccessView*>(Parameter.Resource))
			{
				if (GRHIValidationEnabled)
				{
					RHIValidation::ValidateUnorderedAccessView(RHIShader, Parameter.Index, UAV);
				}
				Tracker->AssertUAV(static_cast<FRHIUnorderedAccessView*>(Parameter.Resource), InRequiredUAVMode, Parameter.Index);
			}
			break;
		case FRHIShaderParameterResource::EType::Sampler:
			// No validation
			break;
		case FRHIShaderParameterResource::EType::UniformBuffer:
			if (FRHIUniformBuffer* UniformBuffer = static_cast<FRHIUniformBuffer*>(Parameter.Resource))
			{
				if (GRHIValidationEnabled)
				{
					RHIValidation::ValidateUniformBuffer(RHIShader, Parameter.Index, UniformBuffer);
				}
				StaticUniformBuffers.ValidateSetShaderUniformBuffer(UniformBuffer);
			}
			break;
		default:
			checkf(false, TEXT("Unhandled resource type?"));
			break;
		}
	}
}

class FValidationComputeContext final : public IRHIComputeContext
{
public:
	enum EType
	{
		Default,
		Parallel
	} const Type;

	FValidationComputeContext(EType Type);

	virtual ~FValidationComputeContext()
	{
	}

	virtual IRHIComputeContext& GetLowestLevelContext() override final
	{
		checkSlow(RHIContext);
		return *RHIContext;
	}

	virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState) override final
	{
		State.bComputePSOSet = true;

		// Reset the compute UAV tracker since the renderer must re-bind all resources after changing a shader.
		Tracker->ResetUAVState(RHIValidation::EUAVMode::Compute);

		State.StaticUniformBuffers.bInSetPipelineStateCall = true;
		RHIContext->RHISetComputePipelineState(ComputePipelineState);
		State.StaticUniformBuffers.bInSetPipelineStateCall = false;
	}

	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) override final
	{
		checkf(State.bComputePSOSet, TEXT("A Compute PSO has to be set to set resources into a shader!"));
		FValidationRHI::ValidateThreadGroupCount(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		RHIContext->RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		Tracker->Dispatch();
	}

	virtual void RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) override final
	{
		checkf(State.bComputePSOSet, TEXT("A Compute PSO has to be set to set resources into a shader!"));
		FValidationRHI::ValidateDispatchIndirectArgsBuffer(ArgumentBuffer, ArgumentOffset);
		Tracker->Assert(ArgumentBuffer->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);

		RHIContext->RHIDispatchIndirectComputeShader(ArgumentBuffer, ArgumentOffset);
		Tracker->Dispatch();
	}

	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) override final
	{
		RHIContext->RHISetAsyncComputeBudget(Budget);
	}

	virtual void RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions) override final
	{
		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingAliases);
		}

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingOperationsBegin);
		}

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingSignals);
		}

		RHIContext->RHIBeginTransitions(Transitions);
	}

	virtual void RHIEndTransitions(TArrayView<const FRHITransition*> Transitions) override final
	{
		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingWaits);
		}

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingOperationsEnd);
		}

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingAliasingOverlaps);
		}

		RHIContext->RHIEndTransitions(Transitions);
	}

	virtual void RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values) override final
	{
		Tracker->Assert(UnorderedAccessViewRHI->GetViewIdentity(), ERHIAccess::UAVCompute);
		RHIContext->RHIClearUAVFloat(UnorderedAccessViewRHI, Values);
	}

	virtual void RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values) override final
	{
		Tracker->Assert(UnorderedAccessViewRHI->GetViewIdentity(), ERHIAccess::UAVCompute);
		RHIContext->RHIClearUAVUint(UnorderedAccessViewRHI, Values);
	}

	virtual void RHISetShaderRootConstants(const FUint32Vector4& Constants) final override
	{
		RHIContext->RHISetShaderRootConstants(Constants);
	}

	virtual void RHIDispatchShaderBundle(
		FRHIShaderBundle* ShaderBundleRHI,
		FRHIShaderResourceView* RecordArgBufferSRV,
		TConstArrayView<FRHIShaderBundleDispatch> Dispatches,
		bool bEmulated) final override
	{
		checkf(Dispatches.Num() > 0, TEXT("A shader bundle must be dispatched with at least one record."));
		for (const FRHIShaderBundleDispatch& Dispatch : Dispatches)
		{
			State.bComputePSOSet = true;

			// Reset the compute UAV tracker since the renderer must re-bind all resources after changing a shader.
			Tracker->ResetUAVState(RHIValidation::EUAVMode::Compute);

			ValidateShaderParameters(Dispatch.Shader, Tracker, State.StaticUniformBuffers, Dispatch.Parameters.ResourceParameters, ERHIAccess::SRVCompute, RHIValidation::EUAVMode::Compute);
			ValidateShaderParameters(Dispatch.Shader, Tracker, State.StaticUniformBuffers, Dispatch.Parameters.BindlessParameters, ERHIAccess::SRVCompute, RHIValidation::EUAVMode::Compute);

			if (bEmulated)
			{
				const uint32 ArgumentOffset = (Dispatch.RecordIndex * FRHIShaderBundle::ArgumentByteStride);
				FValidationRHI::ValidateDispatchIndirectArgsBuffer(RecordArgBufferSRV->GetBuffer(), ArgumentOffset);
			}
		}

		if (bEmulated)
		{
			Tracker->Assert(RecordArgBufferSRV->GetBuffer()->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		}
		else
		{
			Tracker->Assert(RecordArgBufferSRV->GetViewIdentity(),  ERHIAccess::SRVCompute);
		}

		RHIContext->RHIDispatchShaderBundle(ShaderBundleRHI, RecordArgBufferSRV, Dispatches, bEmulated);
	}

	virtual void RHIBeginUAVOverlap() final override
	{
		Tracker->AllUAVsOverlap(true);
		RHIContext->RHIBeginUAVOverlap();
	}

	virtual void RHIEndUAVOverlap() final override
	{
		Tracker->AllUAVsOverlap(false);
		RHIContext->RHIEndUAVOverlap();
	}

	virtual void RHIBeginUAVOverlap(TConstArrayView<FRHIUnorderedAccessView*> UAVs) final override
	{
		for (FRHIUnorderedAccessView* UAV : UAVs)
		{
			Tracker->SpecificUAVOverlap(UAV->GetViewIdentity(), true);
		}
		RHIContext->RHIBeginUAVOverlap(UAVs);
	}

	virtual void RHIEndUAVOverlap(TConstArrayView<FRHIUnorderedAccessView*> UAVs) final override
	{
		for (FRHIUnorderedAccessView* UAV : UAVs)
		{
			Tracker->SpecificUAVOverlap(UAV->GetViewIdentity(), false);
		}
		RHIContext->RHIEndUAVOverlap(UAVs);
	}

	virtual void RHISubmitCommandsHint() override final
	{
		RHIValidation::FTracker::ReplayOpQueue(ERHIPipeline::AsyncCompute, Tracker->Finalize());
		RHIContext->RHISubmitCommandsHint();
	}

	virtual void RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override
	{
		checkf(State.bComputePSOSet, TEXT("A Compute PSO has to be set to set resources into a shader!"));

		ValidateShaderParameters(Shader, Tracker, State.StaticUniformBuffers, InResourceParameters, ERHIAccess::SRVCompute, RHIValidation::EUAVMode::Compute);
		ValidateShaderParameters(Shader, Tracker, State.StaticUniformBuffers, InBindlessParameters, ERHIAccess::SRVCompute, RHIValidation::EUAVMode::Compute);

		RHIContext->RHISetShaderParameters(Shader, InParametersData, InParameters, InResourceParameters, InBindlessParameters);
	}

	virtual void RHISetShaderUnbinds(FRHIComputeShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds) final override
	{
		checkf(State.bComputePSOSet, TEXT("A Compute PSO has to be set to set resources into a shader!"));
		RHIContext->RHISetShaderUnbinds(Shader, InUnbinds);
	}

	virtual void RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers) override final
	{
		InUniformBuffers.Bind(State.StaticUniformBuffers.Bindings);
		RHIContext->RHISetStaticUniformBuffers(InUniformBuffers);
	}

	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) override final
	{
		Tracker->PushBreadcrumb(Name);
		RHIContext->RHIPushEvent(Name, Color);
	}

	virtual void RHIPopEvent() override final
	{
		Tracker->PopBreadcrumb();
		RHIContext->RHIPopEvent();
	}

	virtual void RHIWriteGPUFence(FRHIGPUFence* FenceRHI) override final
	{
		RHIContext->RHIWriteGPUFence(FenceRHI);
	}

	virtual void RHISetGPUMask(FRHIGPUMask GPUMask) override final
	{
		RHIContext->RHISetGPUMask(GPUMask);
	}

	virtual FRHIGPUMask RHIGetGPUMask() const override final
	{
		return RHIContext->RHIGetGPUMask();
	}

	virtual void RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* DestinationStagingBufferRHI, uint32 InOffset, uint32 InNumBytes) override final;

#if WITH_MGPU
	virtual void RHITransferResources(TConstArrayView<FTransferResourceParams> Params)
	{
		RHIContext->RHITransferResources(Params);
	}

	virtual void RHITransferResourceSignal(TConstArrayView<FTransferResourceFenceData*> FenceDatas, FRHIGPUMask SrcGPUMask)
	{
		RHIContext->RHITransferResourceSignal(FenceDatas, SrcGPUMask);
	}

	virtual void RHITransferResourceWait(TConstArrayView<FTransferResourceFenceData*> FenceDatas)
	{
		RHIContext->RHITransferResourceWait(FenceDatas);
	}

	virtual void RHICrossGPUTransfer(TConstArrayView<FTransferResourceParams> Params, TConstArrayView<FCrossGPUTransferFence*> PreTransfer, TConstArrayView<FCrossGPUTransferFence*> PostTransfer)
	{
		RHIContext->RHICrossGPUTransfer(Params, PreTransfer, PostTransfer);
	}

	virtual void RHICrossGPUTransferSignal(TConstArrayView<FTransferResourceParams> Params, TConstArrayView<FCrossGPUTransferFence*> PreTransfer)
	{
		RHIContext->RHICrossGPUTransferSignal(Params, PreTransfer);
	}

	virtual void RHICrossGPUTransferWait(TConstArrayView<FCrossGPUTransferFence*> SyncPoints)
	{
		RHIContext->RHICrossGPUTransferWait(SyncPoints);
	}
#endif // WITH_MGPU

	virtual void RHIBuildAccelerationStructures(TConstArrayView<FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange) override final
	{
		// #yuriy_todo: explicit transitions and state validation for BLAS
		RHIContext->RHIBuildAccelerationStructures(Params, ScratchBufferRange);
	}

	virtual void RHIBuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams) override final
	{
		// #yuriy_todo: validate all referenced BLAS states
		if (SceneBuildParams.Scene)
		{
			Tracker->Assert(SceneBuildParams.Scene->GetWholeResourceIdentity(), ERHIAccess::BVHWrite);
		}
		RHIContext->RHIBuildAccelerationStructure(SceneBuildParams);
	}

	virtual void RHIBindAccelerationStructureMemory(FRHIRayTracingScene* Scene, FRHIBuffer* Buffer, uint32 BufferOffset) override final
	{
		RHIContext->RHIBindAccelerationStructureMemory(Scene, Buffer, BufferOffset);
	}

	inline void LinkToContext(IRHIComputeContext* PlatformContext)
	{
		RHIContext = PlatformContext;
		PlatformContext->WrappingContext = this;
		PlatformContext->Tracker = &State.TrackerInstance;
	}

	IRHIComputeContext* RHIContext = nullptr;

protected:
	struct FState
	{
		RHIValidation::FTracker TrackerInstance{ ERHIPipeline::AsyncCompute };
		RHIValidation::FStaticUniformBuffers StaticUniformBuffers;

		FString ComputePassName;
		bool bComputePSOSet{};

		void Reset();
	} State;

	friend class FValidationRHI;
};

class FValidationContext final : public IRHICommandContext
{
public:
	enum EType
	{
		Default,
		Parallel
	} const Type;

	FValidationContext(EType InType);

	virtual IRHIComputeContext& GetLowestLevelContext() override final
	{
		checkSlow(RHIContext);
		return *RHIContext;
	}

	virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState) override final
	{
		State.bComputePSOSet = true;

		// Reset the compute UAV tracker since the renderer must re-bind all resources after changing a shader.
		Tracker->ResetUAVState(RHIValidation::EUAVMode::Compute);

		State.StaticUniformBuffers.bInSetPipelineStateCall = true;
		RHIContext->RHISetComputePipelineState(ComputePipelineState);
		State.StaticUniformBuffers.bInSetPipelineStateCall = false;
	}

	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) override final
	{
		checkf(State.bComputePSOSet, TEXT("A Compute PSO has to be set to set resources into a shader!"));
		FValidationRHI::ValidateThreadGroupCount(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		RHIContext->RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		Tracker->Dispatch();
	}

	virtual void RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) override final
	{
		checkf(State.bComputePSOSet, TEXT("A Compute PSO has to be set to set resources into a shader!"));
		FValidationRHI::ValidateDispatchIndirectArgsBuffer(ArgumentBuffer, ArgumentOffset);
		Tracker->Assert(ArgumentBuffer->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);

		RHIContext->RHIDispatchIndirectComputeShader(ArgumentBuffer, ArgumentOffset);
		Tracker->Dispatch();
	}

	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) override final
	{
		RHIContext->RHISetAsyncComputeBudget(Budget);
	}

	// Useful when used with geometry shader (emit polygons to different viewports), otherwise SetViewPort() is simpler
	// @param Count >0
	// @param Data must not be 0
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) override final
	{
		RHIContext->RHISetMultipleViewports(Count, Data);
	}

	virtual void RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values) final override
	{
		// @todo should we assert here? If the base RHI uses a compute shader via
		// FRHICommandList_RecursiveHazardous then we might double-assert which breaks the tracking
		RHIContext->RHIClearUAVFloat(UnorderedAccessViewRHI, Values);
	}

	virtual void RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values) final override
	{
		// @todo should we assert here? If the base RHI uses a compute shader via
		// FRHICommandList_RecursiveHazardous then we might double-assert which breaks the tracking
		RHIContext->RHIClearUAVUint(UnorderedAccessViewRHI, Values);
	}

	virtual void RHISetShaderRootConstants(const FUint32Vector4& Constants) final override
	{
		RHIContext->RHISetShaderRootConstants(Constants);
	}

	virtual void RHIDispatchShaderBundle(
		FRHIShaderBundle* ShaderBundleRHI,
		FRHIShaderResourceView* RecordArgBufferSRV,
		TConstArrayView<FRHIShaderBundleDispatch> Dispatches,
		bool bEmulated) final override
	{
		checkf(Dispatches.Num() > 0, TEXT("A shader bundle must be dispatched with at least one record."));
		for (const FRHIShaderBundleDispatch& Dispatch : Dispatches)
		{
			State.bComputePSOSet = true;

			// Reset the compute UAV tracker since the renderer must re-bind all resources after changing a shader.
			Tracker->ResetUAVState(RHIValidation::EUAVMode::Compute);

			ValidateShaderParameters(Dispatch.Shader, Tracker, State.StaticUniformBuffers, Dispatch.Parameters.ResourceParameters, ERHIAccess::SRVCompute, RHIValidation::EUAVMode::Compute);
			ValidateShaderParameters(Dispatch.Shader, Tracker, State.StaticUniformBuffers, Dispatch.Parameters.BindlessParameters, ERHIAccess::SRVCompute, RHIValidation::EUAVMode::Compute);

			if (bEmulated)
			{
				const uint32 ArgumentOffset = (Dispatch.RecordIndex * FRHIShaderBundle::ArgumentByteStride);
				FValidationRHI::ValidateDispatchIndirectArgsBuffer(RecordArgBufferSRV->GetBuffer(), ArgumentOffset);
			}
		}

		if (bEmulated)
		{
			Tracker->Assert(RecordArgBufferSRV->GetBuffer()->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		}
		else
		{
			Tracker->Assert(RecordArgBufferSRV->GetViewIdentity(),  ERHIAccess::SRVCompute);
		}

		RHIContext->RHIDispatchShaderBundle(ShaderBundleRHI, RecordArgBufferSRV, Dispatches, bEmulated);
	}

	virtual void RHIBeginUAVOverlap() final override
	{
		Tracker->AllUAVsOverlap(true);
		RHIContext->RHIBeginUAVOverlap();
	}

	virtual void RHIEndUAVOverlap() final override
	{
		Tracker->AllUAVsOverlap(false);
		RHIContext->RHIEndUAVOverlap();
	}

	virtual void RHIBeginUAVOverlap(TConstArrayView<FRHIUnorderedAccessView*> UAVs) final override
	{
		for (FRHIUnorderedAccessView* UAV : UAVs)
		{
			Tracker->SpecificUAVOverlap(UAV->GetViewIdentity(), true);
		}
		RHIContext->RHIBeginUAVOverlap(UAVs);
	}

	virtual void RHIEndUAVOverlap(TConstArrayView<FRHIUnorderedAccessView*> UAVs) final override
	{
		for (FRHIUnorderedAccessView* UAV : UAVs)
		{
			Tracker->SpecificUAVOverlap(UAV->GetViewIdentity(), false);
		}
		RHIContext->RHIEndUAVOverlap(UAVs);
	}

	virtual void RHIResummarizeHTile(FRHITexture2D* DepthTexture) override final
	{
		Tracker->Assert(DepthTexture->GetWholeResourceIdentity(), ERHIAccess::DSVWrite);
		RHIContext->RHIResummarizeHTile(DepthTexture);
	}

	virtual void* RHIGetNativeCommandBuffer() override final
	{
		return RHIContext->RHIGetNativeCommandBuffer();
	}

	virtual void RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions) override final
	{
		ensureMsgf(!State.bInsideBeginRenderPass, TEXT("Beginning a transition within a renderpass is not supported!"));

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingAliases);
		}

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingOperationsBegin);
		}

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingSignals);
		}

		RHIContext->RHIBeginTransitions(Transitions);
	}

	virtual void RHIEndTransitions(TArrayView<const FRHITransition*> Transitions) override final
	{
		ensureMsgf(!State.bInsideBeginRenderPass, TEXT("Ending a transition within a renderpass is not supported!"));

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingWaits);
		}

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingOperationsEnd);
		}

		for (const FRHITransition* Transition : Transitions)
		{
			Tracker->AddOps(Transition->PendingAliasingOverlaps);
		}

		RHIContext->RHIEndTransitions(Transitions);
	}

	virtual void SetTrackedAccess(const FRHITrackedAccessInfo& Info) override final
	{
		check(Info.Resource != nullptr);
		check(Info.Access != ERHIAccess::Unknown);
		checkf(Type != EType::Parallel,
			TEXT("SetTrackedAccess(%s, %s) was called from a parallel translate context. This is not allowed. This is most likely a call to RHICmdList.Transition on a command list queued for parallel dispatch."),
			*Info.Resource->GetName().ToString(),
			*GetRHIAccessName(Info.Access));

		Tracker->SetTrackedAccess(Info.Resource->GetValidationTrackerResource(), Info.Access);

		RHIContext->SetTrackedAccess(Info);
	}

	virtual void RHIBeginRenderQuery(FRHIRenderQuery* RenderQuery) override final
	{
		RHIContext->RHIBeginRenderQuery(RenderQuery);
	}

	virtual void RHIEndRenderQuery(FRHIRenderQuery* RenderQuery) override final
	{
		RHIContext->RHIEndRenderQuery(RenderQuery);
	}

	virtual void RHICalibrateTimers(FRHITimestampCalibrationQuery* CalibrationQuery) override final
	{
		RHIContext->RHICalibrateTimers(CalibrationQuery);
	}

	virtual void RHISubmitCommandsHint() override final
	{
		ensureMsgf(!State.bInsideBeginRenderPass, TEXT("Submitting inside a RenderPass is not efficient!"));
		RHIContext->RHISubmitCommandsHint();
		RHIValidation::FTracker::ReplayOpQueue(ERHIPipeline::Graphics, Tracker->Finalize());
	}

	// Used for OpenGL to check and see if any occlusion queries can be read back on the RHI thread. If they aren't ready when we need them, then we end up stalling.
	virtual void RHIPollOcclusionQueries() override final
	{
		RHIContext->RHIPollOcclusionQueries();
	}

	// Not all RHIs need this (Mobile specific)
	virtual void RHIDiscardRenderTargets(bool bDepth, bool bStencil, uint32 ColorBitMask) override final
	{
		RHIContext->RHIDiscardRenderTargets(bDepth, bStencil, ColorBitMask);
	}

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginDrawingViewport(FRHIViewport* Viewport, FRHITexture* RenderTargetRHI) override final
	{
		RHIContext->RHIBeginDrawingViewport(Viewport, RenderTargetRHI);
	}

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync) override final
	{
		RHIContext->RHIEndDrawingViewport(Viewport, bPresent, bLockToVsync);
	}

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginFrame() override final;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndFrame() override final;

	/**
	* Signals the beginning of scene rendering. The RHI makes certain caching assumptions between
	* calls to BeginScene/EndScene. Currently the only restriction is that you can't update texture
	* references.
	*/
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginScene() override final
	{
		RHIContext->RHIBeginScene();
	}

	/**
	* Signals the end of scene rendering. See RHIBeginScene.
	*/
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndScene() override final
	{
		RHIContext->RHIEndScene();
	}

	virtual void RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset) override final
	{
		//#todo-rco: Decide if this is needed or not...
		//checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to set-up the vertex streams!"));

		// @todo: do we really need to allow setting nullptr stream sources anymore?
		if (VertexBuffer)
		{
		checkf(State.bInsideBeginRenderPass, TEXT("A RenderPass has to be set to set-up the vertex streams!"));
			Tracker->Assert(VertexBuffer->GetWholeResourceIdentity(), ERHIAccess::VertexOrIndexBuffer);
		}

		RHIContext->RHISetStreamSource(StreamIndex, VertexBuffer, Offset);
	}

	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) override final
	{
		RHIContext->RHISetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
	}

	virtual void RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ) override final
	{
		RHIContext->RHISetStereoViewport(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
	}

	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) override final
	{
		RHIContext->RHISetScissorRect(bEnable, MinX, MinY, MaxX, MaxY);
	}

	virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState) override final
	{
		checkf(State.bInsideBeginRenderPass, TEXT("Graphics PSOs can only be set inside a RenderPass!"));
		State.bGfxPSOSet = true;
		State.bComputePSOSet = false;

		ValidateDepthStencilForSetGraphicsPipelineState(GraphicsState->DSMode);

		// Setting a new PSO unbinds all previous bound resources
		Tracker->ResetUAVState(RHIValidation::EUAVMode::Graphics);

		State.StaticUniformBuffers.bInSetPipelineStateCall = true;
		RHIContext->RHISetGraphicsPipelineState(GraphicsState, StencilRef, bApplyAdditionalState);
		State.StaticUniformBuffers.bInSetPipelineStateCall = false;
	}

#if PLATFORM_USE_FALLBACK_PSO
	virtual void RHISetGraphicsPipelineState(const FGraphicsPipelineStateInitializer& PsoInit, uint32 StencilRef, bool bApplyAdditionalState) override final
	{
		checkf(State.bInsideBeginRenderPass, TEXT("Graphics PSOs can only be set inside a RenderPass!"));
		State.bGfxPSOSet = true;
		State.bComputePSOSet = false;

		ValidateDepthStencilForSetGraphicsPipelineState(PsoInit.DepthStencilState->ActualDSMode);

		// Setting a new PSO unbinds all previous bound resources
		Tracker->ResetUAVState(RHIValidation::EUAVMode::Graphics);

		State.StaticUniformBuffers.bInSetPipelineStateCall = true;
		RHIContext->RHISetGraphicsPipelineState(PsoInit, StencilRef, bApplyAdditionalState);
		State.StaticUniformBuffers.bInSetPipelineStateCall = false;
	}
#endif

	virtual void RHISetShaderParameters(FRHIGraphicsShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to set resources into a shader!"));

		ValidateShaderParameters(Shader, Tracker, State.StaticUniformBuffers, InResourceParameters, ERHIAccess::SRVGraphics, RHIValidation::EUAVMode::Graphics);
		ValidateShaderParameters(Shader, Tracker, State.StaticUniformBuffers, InBindlessParameters, ERHIAccess::SRVGraphics, RHIValidation::EUAVMode::Graphics);

		RHIContext->RHISetShaderParameters(Shader, InParametersData, InParameters, InResourceParameters, InBindlessParameters);
	}

	virtual void RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override
	{
		checkf(State.bComputePSOSet, TEXT("A Compute PSO has to be set to set resources into a shader!"));

		ValidateShaderParameters(Shader, Tracker, State.StaticUniformBuffers, InResourceParameters, ERHIAccess::SRVCompute, RHIValidation::EUAVMode::Compute);
		ValidateShaderParameters(Shader, Tracker, State.StaticUniformBuffers, InBindlessParameters, ERHIAccess::SRVCompute, RHIValidation::EUAVMode::Compute);

		RHIContext->RHISetShaderParameters(Shader, InParametersData, InParameters, InResourceParameters, InBindlessParameters);
	}

	virtual void RHISetShaderUnbinds(FRHIGraphicsShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds) override final
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to set resources into a shader!"));
		RHIContext->RHISetShaderUnbinds(Shader, InUnbinds);
	}

	virtual void RHISetShaderUnbinds(FRHIComputeShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds) override final
	{
		checkf(State.bComputePSOSet, TEXT("A Compute PSO has to be set to set resources into a shader!"));
		RHIContext->RHISetShaderUnbinds(Shader, InUnbinds);
	}

	virtual void RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers) override final
	{
		InUniformBuffers.Bind(State.StaticUniformBuffers.Bindings);
		RHIContext->RHISetStaticUniformBuffers(InUniformBuffers);
	}

	virtual void RHISetStencilRef(uint32 StencilRef) override final
	{
		//checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to change stencil ref!"));
		RHIContext->RHISetStencilRef(StencilRef);
	}

	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) override final
	{
		//checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to change blend factor!"));
		RHIContext->RHISetBlendFactor(BlendFactor);
	}

	virtual void RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) override final
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to draw!"));
		SetupDrawing();
		RHIContext->RHIDrawPrimitive(BaseVertexIndex, NumPrimitives, NumInstances);
		Tracker->Draw();
	}

	virtual void RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) override final
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to draw!"));
		FValidationRHI::ValidateIndirectArgsBuffer(ArgumentBuffer, ArgumentOffset, sizeof(FRHIDrawIndirectParameters), 0);
		SetupDrawing();
		Tracker->Assert(ArgumentBuffer->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		RHIContext->RHIDrawPrimitiveIndirect(ArgumentBuffer, ArgumentOffset);
		Tracker->Draw();
	}

	virtual void RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) override final
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to draw!"));
		FValidationRHI::ValidateIndirectArgsBuffer(ArgumentsBufferRHI, DrawArgumentsIndex * ArgumentsBufferRHI->GetStride(), sizeof(FRHIDrawIndexedIndirectParameters), 0);
		SetupDrawing();
		Tracker->Assert(ArgumentsBufferRHI->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		Tracker->Assert(IndexBufferRHI->GetWholeResourceIdentity(), ERHIAccess::VertexOrIndexBuffer);
		RHIContext->RHIDrawIndexedIndirect(IndexBufferRHI, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
		Tracker->Draw();
	}

	// @param NumPrimitives need to be >0 
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) override final
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to draw!"));
		checkf(EnumHasAnyFlags(IndexBuffer->GetUsage(), EBufferUsageFlags::IndexBuffer), TEXT("The buffer '%s' is used as an index buffer, but was not created with the IndexBuffer flag."), *IndexBuffer->GetName().ToString());
		SetupDrawing();
		Tracker->Assert(IndexBuffer->GetWholeResourceIdentity(), ERHIAccess::VertexOrIndexBuffer);
		RHIContext->RHIDrawIndexedPrimitive(IndexBuffer, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
		Tracker->Draw();
	}

	virtual void RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) override final
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to draw!"));
		checkf(EnumHasAnyFlags(IndexBuffer->GetUsage(), EBufferUsageFlags::IndexBuffer), TEXT("The buffer '%s' is used as an index buffer, but was not created with the IndexBuffer flag."), *IndexBuffer->GetName().ToString());
		FValidationRHI::ValidateIndirectArgsBuffer(ArgumentBuffer, ArgumentOffset, sizeof(FRHIDrawIndexedIndirectParameters), 0);
		SetupDrawing();
		Tracker->Assert(ArgumentBuffer->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		Tracker->Assert(IndexBuffer->GetWholeResourceIdentity(), ERHIAccess::VertexOrIndexBuffer);
		RHIContext->RHIDrawIndexedPrimitiveIndirect(IndexBuffer, ArgumentBuffer, ArgumentOffset);
		Tracker->Draw();
	}

	virtual void RHIMultiDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset, FRHIBuffer* CountBuffer, uint32 CountBufferOffset, uint32 MaxDrawArguments) override final
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to draw!"));
		checkf(EnumHasAnyFlags(IndexBuffer->GetUsage(), EBufferUsageFlags::IndexBuffer), TEXT("The buffer '%s' is used as an index buffer, but was not created with the IndexBuffer flag."), *IndexBuffer->GetName().ToString());
		FValidationRHI::ValidateIndirectArgsBuffer(ArgumentBuffer, ArgumentOffset, sizeof(FRHIDrawIndexedIndirectParameters), 0);
		SetupDrawing();
		Tracker->Assert(ArgumentBuffer->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		if (CountBuffer)
		{
			Tracker->Assert(CountBuffer->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		}
		Tracker->Assert(IndexBuffer->GetWholeResourceIdentity(), ERHIAccess::VertexOrIndexBuffer);
		RHIContext->RHIMultiDrawIndexedPrimitiveIndirect(IndexBuffer, ArgumentBuffer, ArgumentOffset, CountBuffer, CountBufferOffset, MaxDrawArguments);
		Tracker->Draw();
	}

	virtual void RHIDispatchMeshShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to draw!"));
		FValidationRHI::ValidateThreadGroupCount(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		SetupDrawing();
		RHIContext->RHIDispatchMeshShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		Tracker->Draw();
	}

	virtual void RHIDispatchIndirectMeshShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{
		checkf(State.bGfxPSOSet, TEXT("A Graphics PSO has to be set to draw!"));
		FValidationRHI::ValidateDispatchIndirectArgsBuffer(ArgumentBuffer, ArgumentOffset);
		SetupDrawing();
		Tracker->Assert(ArgumentBuffer->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		RHIContext->RHIDispatchIndirectMeshShader(ArgumentBuffer, ArgumentOffset);
		Tracker->Draw();
	}

	/**
	* Sets Depth Bounds range with the given min/max depth.
	* @param MinDepth	The minimum depth for depth bounds test
	* @param MaxDepth	The maximum depth for depth bounds test.
	*					The valid values for fMinDepth and fMaxDepth are such that 0 <= fMinDepth <= fMaxDepth <= 1
	*/
	virtual void RHISetDepthBounds(float MinDepth, float MaxDepth) override final
	{
		checkf(MinDepth >= 0.f && MinDepth <= 1.f, TEXT("Depth bounds min of %f is outside allowed range of [0, 1]"), MinDepth);
		checkf(MaxDepth >= 0.f && MaxDepth <= 1.f, TEXT("Depth bounds max of %f is outside allowed range of [0, 1]"), MaxDepth);
		RHIContext->RHISetDepthBounds(MinDepth, MaxDepth);
	}

	virtual void RHISetShadingRate(EVRSShadingRate ShadingRate, EVRSRateCombiner Combiner) override final
	{
		RHIContext->RHISetShadingRate(ShadingRate, Combiner);
	}

	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) override final
	{
		Tracker->PushBreadcrumb(Name);
		RHIContext->RHIPushEvent(Name, Color);
	}

	virtual void RHIPopEvent() override final
	{
		Tracker->PopBreadcrumb();
		RHIContext->RHIPopEvent();
	}

	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName) override final
	{
		checkf(!State.bInsideBeginRenderPass, TEXT("Trying to begin RenderPass '%s', but already inside '%s'!"), *State.RenderPassName, InName);
		checkf(InName!=nullptr, TEXT("RenderPass should have a name!"));
		State.bInsideBeginRenderPass = true;
		State.RenderPassInfo = InInfo;
		State.RenderPassName = InName;

		FIntVector ViewDimensions = FIntVector(0);

		// assert that render targets are writable
		for (int32 RTVIndex = 0; RTVIndex < MaxSimultaneousRenderTargets; ++RTVIndex)
		{
			FRHIRenderPassInfo::FColorEntry& RTV = State.RenderPassInfo.ColorRenderTargets[RTVIndex];
			if (RTV.RenderTarget == nullptr)
			{
				checkf(RTV.ResolveTarget == nullptr, TEXT("Render target is null, but resolve target is not."));
				continue;
			}

			// Check all bound textures have the same dimensions
			FIntVector MipDimensions = RTV.RenderTarget->GetMipDimensions(RTV.MipIndex);
			checkf(ViewDimensions.IsZero() || ViewDimensions == MipDimensions, TEXT("Render target size mismatch (RT%d: %dx%d vs. Expected: %dx%d). All render and depth target views must have the same effective dimensions."), RTVIndex, MipDimensions.X, MipDimensions.Y, ViewDimensions.X, ViewDimensions.Y);
			ViewDimensions = MipDimensions;

			uint32 ArraySlice = RTV.ArraySlice;
			uint32 NumArraySlices = 1;
			if (RTV.ArraySlice < 0)
			{
				ArraySlice = 0;
				NumArraySlices = 0;
			}

			Tracker->Assert(RTV.RenderTarget->GetViewIdentity(RTV.MipIndex, 1, ArraySlice, NumArraySlices, 0, 0), ERHIAccess::RTV);

			if (RTV.ResolveTarget)
			{
				const FRHITextureDesc& RenderTargetDesc = RTV.RenderTarget->GetDesc();
				const FRHITextureDesc& ResolveTargetDesc = RTV.ResolveTarget->GetDesc();

				checkf(RenderTargetDesc.Extent == ResolveTargetDesc.Extent, TEXT("Render target extent must match resolve target extent."));
				checkf(RenderTargetDesc.Format == ResolveTargetDesc.Format, TEXT("Render target format must match resolve target format."));

				Tracker->Assert(RTV.ResolveTarget->GetViewIdentity(RTV.MipIndex, 1, ArraySlice, NumArraySlices, 0, 0), ERHIAccess::ResolveDst);
			}
		}

		FRHIRenderPassInfo::FDepthStencilEntry& DSV = State.RenderPassInfo.DepthStencilRenderTarget;

		if (DSV.DepthStencilTarget)
		{
			// Check all bound textures have the same dimensions
			FIntVector MipDimensions = DSV.DepthStencilTarget->GetMipDimensions(0);
			checkf(ViewDimensions.IsZero() || ViewDimensions == MipDimensions, TEXT("Depth target size mismatch (Depth: %dx%d vs. Expected: %dx%d). All render and depth target views must have the same effective dimensions."), MipDimensions.X, MipDimensions.Y, ViewDimensions.X, ViewDimensions.Y);
			ViewDimensions = MipDimensions;

			if (DSV.ResolveTarget)
			{
				const FRHITextureDesc& DepthStencilTargetDesc = DSV.DepthStencilTarget->GetDesc();
				const FRHITextureDesc& ResolveTargetDesc = DSV.ResolveTarget->GetDesc();

				checkf(DepthStencilTargetDesc.Extent == ResolveTargetDesc.Extent, TEXT("Depth stencil target extent must match resolve target extent."));
				checkf(DepthStencilTargetDesc.IsTexture2D() && ResolveTargetDesc.IsTexture2D(), TEXT("Only 2D depth stencil resolves are supported."));
			}
		}

		// @todo: additional checks for matching array slice counts on RTVs/DSVs

		// assert depth is in the correct mode
		if (DSV.ExclusiveDepthStencil.IsUsingDepth())
		{
			ERHIAccess DepthAccess = DSV.ExclusiveDepthStencil.IsDepthWrite()
				? ERHIAccess::DSVWrite
				: ERHIAccess::DSVRead;

			checkf(DSV.DepthStencilTarget, TEXT("Depth read/write is enabled but no depth stencil texture is bound."));
			Tracker->Assert(DSV.DepthStencilTarget->GetViewIdentity(0, 0, 0, 0, uint32(RHIValidation::EResourcePlane::Common), 1), DepthAccess);

			if (DSV.ResolveTarget)
			{
				Tracker->Assert(DSV.ResolveTarget->GetViewIdentity(0, 0, 0, 0, uint32(RHIValidation::EResourcePlane::Common), 1), ERHIAccess::ResolveDst);
			}
		}

		// assert stencil is in the correct mode
		if (DSV.ExclusiveDepthStencil.IsUsingStencil())
		{
			ERHIAccess StencilAccess = DSV.ExclusiveDepthStencil.IsStencilWrite()
				? ERHIAccess::DSVWrite
				: ERHIAccess::DSVRead;

			checkf(DSV.DepthStencilTarget, TEXT("Stencil read/write is enabled but no depth stencil texture is bound."));

			bool bIsStencilFormat = IsStencilFormat(DSV.DepthStencilTarget->GetFormat());
			checkf(bIsStencilFormat, TEXT("Stencil read/write is enabled but depth stencil texture doesn't have a stencil plane."));
			if (bIsStencilFormat)
			{
				Tracker->Assert(DSV.DepthStencilTarget->GetViewIdentity(0, 0, 0, 0, uint32(RHIValidation::EResourcePlane::Stencil), 1), StencilAccess);

				if (DSV.ResolveTarget)
				{
					Tracker->Assert(DSV.ResolveTarget->GetViewIdentity(0, 0, 0, 0, uint32(RHIValidation::EResourcePlane::Stencil), 1), ERHIAccess::ResolveDst);
				}
			}
		}

		// assert shading-rate attachment is in the correct mode and format.
		if (State.RenderPassInfo.ShadingRateTexture.IsValid())
		{
			FTextureRHIRef ShadingRateTexture = State.RenderPassInfo.ShadingRateTexture;
			checkf(ShadingRateTexture->GetFormat() == GRHIVariableRateShadingImageFormat, TEXT("Shading rate texture is bound, but is not the correct format for this RHI."));
			Tracker->Assert(ShadingRateTexture->GetViewIdentity(0, 0, 0, 0, 0, 0), ERHIAccess::ShadingRateSource);
		}

		RHIContext->RHIBeginRenderPass(InInfo, InName);
	}

	virtual void RHIEndRenderPass() override final
	{
		checkf(State.bInsideBeginRenderPass, TEXT("Trying to end a RenderPass but not inside one!"));
		RHIContext->RHIEndRenderPass();
		State.bInsideBeginRenderPass = false;
		State.PreviousRenderPassName = State.RenderPassName;
	}

	virtual void RHIWriteGPUFence(FRHIGPUFence* FenceRHI) override final
	{
		RHIContext->RHIWriteGPUFence(FenceRHI);
	}

	virtual void RHISetGPUMask(FRHIGPUMask GPUMask) override final
	{
		RHIContext->RHISetGPUMask(GPUMask);
	}

	virtual FRHIGPUMask RHIGetGPUMask() const override final
	{
		return RHIContext->RHIGetGPUMask();
	}

	virtual void RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* DestinationStagingBufferRHI, uint32 InOffset, uint32 InNumBytes) override final;

	virtual void RHICopyTexture(FRHITexture* SourceTexture, FRHITexture* DestTexture, const FRHICopyTextureInfo& CopyInfo) override final
	{
		ensureMsgf(!State.bInsideBeginRenderPass, TEXT("Copying inside a RenderPass is not efficient!"));

		// @todo: do we need to pick subresource, not just whole resource identity here.
		// also, is CopySrc / CopyDest correct?
		Tracker->Assert(SourceTexture->GetWholeResourceIdentity(), ERHIAccess::CopySrc);
		Tracker->Assert(DestTexture->GetWholeResourceIdentity(), ERHIAccess::CopyDest);

		FValidationRHIUtils::ValidateCopyTexture(SourceTexture, DestTexture, CopyInfo);
		RHIContext->RHICopyTexture(SourceTexture, DestTexture, CopyInfo);
	}

	virtual void RHICopyBufferRegion(FRHIBuffer* DestBuffer, uint64 DstOffset, FRHIBuffer* SourceBuffer, uint64 SrcOffset, uint64 NumBytes)
	{
		Tracker->Assert(SourceBuffer->GetWholeResourceIdentity(), ERHIAccess::CopySrc);
		Tracker->Assert(DestBuffer->GetWholeResourceIdentity(), ERHIAccess::CopyDest);
		RHIContext->RHICopyBufferRegion(DestBuffer, DstOffset, SourceBuffer, SrcOffset, NumBytes);
	}

	void RHIClearRayTracingBindings(FRHIRayTracingScene* Scene)
	{
		RHIContext->RHIClearRayTracingBindings(Scene);
	}

	virtual void RHIBuildAccelerationStructures(TConstArrayView<FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange) override final
	{
		// #yuriy_todo: explicit transitions and state validation for BLAS
		RHIContext->RHIBuildAccelerationStructures(Params, ScratchBufferRange);
	}

	virtual void RHIBuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams) override final
	{
		// #yuriy_todo: validate all referenced BLAS states
		if (SceneBuildParams.Scene)
		{
			Tracker->Assert(SceneBuildParams.Scene->GetWholeResourceIdentity(), ERHIAccess::BVHWrite);
		}
		RHIContext->RHIBuildAccelerationStructure(SceneBuildParams);
	}

	virtual void RHIBindAccelerationStructureMemory(FRHIRayTracingScene* Scene, FRHIBuffer* Buffer, uint32 BufferOffset) override final
	{
		RHIContext->RHIBindAccelerationStructureMemory(Scene, Buffer, BufferOffset);
	}

	virtual void RHIRayTraceDispatch(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIRayTracingScene* Scene,
		const FRayTracingShaderBindings& GlobalResourceBindings,
		uint32 Width, uint32 Height) override final
	{
		RHIContext->RHIRayTraceDispatch(RayTracingPipelineState, RayGenShader, Scene, GlobalResourceBindings, Width, Height);
	}

	virtual void RHIRayTraceDispatchIndirect(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIRayTracingScene* Scene,
		const FRayTracingShaderBindings& GlobalResourceBindings,
		FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) override final
	{
		FValidationRHI::ValidateDispatchIndirectArgsBuffer(ArgumentBuffer, ArgumentOffset);
		Tracker->Assert(ArgumentBuffer->GetWholeResourceIdentity(), ERHIAccess::IndirectArgs);
		Tracker->Assert(ArgumentBuffer->GetWholeResourceIdentity(), ERHIAccess::SRVCompute);

		RHIContext->RHIRayTraceDispatchIndirect(RayTracingPipelineState, RayGenShader, Scene, GlobalResourceBindings, ArgumentBuffer, ArgumentOffset);
	}

	virtual void RHISetRayTracingBindings(FRHIRayTracingScene* Scene, FRHIRayTracingPipelineState* Pipeline, uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings, ERayTracingBindingType BindingType) override final
	{
		RHIContext->RHISetRayTracingBindings(Scene, Pipeline, NumBindings, Bindings, BindingType);
	}

	virtual void RHISetRayTracingHitGroup(
		FRHIRayTracingScene* Scene, uint32 InstanceIndex, uint32 SegmentIndex, uint32 ShaderSlot,
		FRHIRayTracingPipelineState* Pipeline, uint32 HitGroupIndex,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 LooseParameterDataSize, const void* LooseParameterData,
		uint32 UserData) override final
	{
		RHIContext->RHISetRayTracingHitGroup(Scene, InstanceIndex, SegmentIndex, ShaderSlot, Pipeline, HitGroupIndex, NumUniformBuffers, UniformBuffers, LooseParameterDataSize, LooseParameterData, UserData);
	}

	virtual void RHISetRayTracingCallableShader(
		FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
		FRHIRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 UserData) override final
	{
		RHIContext->RHISetRayTracingCallableShader(Scene, ShaderSlotInScene, Pipeline, ShaderIndexInPipeline, NumUniformBuffers, UniformBuffers, UserData);
	}

	virtual void RHISetRayTracingMissShader(
		FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
		FRHIRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 UserData) override final
	{
		RHIContext->RHISetRayTracingMissShader(Scene, ShaderSlotInScene, Pipeline, ShaderIndexInPipeline, NumUniformBuffers, UniformBuffers, UserData);
	}

	virtual void StatsSetCategory(FRHIDrawStats* InStats, uint32 InCategoryID) final override
	{
		RHIContext->StatsSetCategory(InStats, InCategoryID);
	}

	void SetupDrawing()
	{
		// nothing to validate right now
	}

	IRHICommandContext* RHIContext = nullptr;

	inline void LinkToContext(IRHICommandContext* PlatformContext)
	{
		RHIContext = PlatformContext;
		PlatformContext->WrappingContext = this;
		PlatformContext->Tracker = &State.TrackerInstance;
	}

	inline void FlushValidationOps()
	{
		RHIValidation::FTracker::ReplayOpQueue(ERHIPipeline::Graphics, Tracker->Finalize());
	}

protected:
	struct FState
	{
		RHIValidation::FTracker TrackerInstance{ ERHIPipeline::Graphics };
		RHIValidation::FStaticUniformBuffers StaticUniformBuffers;

		void* PreviousBeginFrame = nullptr;
		void* PreviousEndFrame = nullptr;
		int32 BeginEndFrameCounter = 0;

		FRHIRenderPassInfo RenderPassInfo;
		FString RenderPassName;
		FString PreviousRenderPassName;
		FString ComputePassName;
		bool bGfxPSOSet{};
		bool bComputePSOSet{};
		bool bInsideBeginRenderPass{};

		void Reset();
	} State;

	friend class FValidationRHI;

private:
	void ValidateDepthStencilForSetGraphicsPipelineState(const FExclusiveDepthStencil& DSMode)
	{
		FRHIRenderPassInfo::FDepthStencilEntry& DSV = State.RenderPassInfo.DepthStencilRenderTarget;

		// assert depth is in the correct mode
		if (DSMode.IsUsingDepth())
		{
			checkf(DSV.ExclusiveDepthStencil.IsUsingDepth(), TEXT("Graphics PSO is using depth but it's not enabled on the RenderPass."));
			checkf(DSMode.IsDepthRead() || DSV.ExclusiveDepthStencil.IsDepthWrite(), TEXT("Graphics PSO is writing to depth but RenderPass depth is ReadOnly."));
		}

		// assert stencil is in the correct mode
		if (DSMode.IsUsingStencil())
		{
			checkf(DSV.ExclusiveDepthStencil.IsUsingStencil(), TEXT("Graphics PSO is using stencil but it's not enabled on the RenderPass."));
			checkf(DSMode.IsStencilRead() || DSV.ExclusiveDepthStencil.IsStencilWrite(), TEXT("Graphics PSO is writing to stencil but RenderPass stencil is ReadOnly."));
		}
	}
};

#endif	// ENABLE_RHI_VALIDATION
