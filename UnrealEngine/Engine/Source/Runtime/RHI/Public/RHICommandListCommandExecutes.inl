// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandListCommandExecutes.inl: RHI Command List execute functions.
=============================================================================*/

#if !defined(INTERNAL_DECORATOR)
	#define INTERNAL_DECORATOR(Method) CmdList.GetContext().Method
#endif

//for functions where the signatures do not match between gfx and compute commandlists
#if !defined(INTERNAL_DECORATOR_COMPUTE)
#define INTERNAL_DECORATOR_COMPUTE(Method) CmdList.GetComputeContext().Method
#endif

#include "RHIResourceUpdates.h"

class FRHICommandListBase;
class IRHIComputeContext;
struct FComputedBSS;
struct FComputedUniformBuffer;
struct FMemory;
struct FRHICommandBeginDrawingViewport;
struct FRHICommandBeginFrame;
struct FRHICommandBeginRenderQuery;
struct FRHICommandBeginScene;
struct FRHICommandBuildLocalBoundShaderState;
struct FRHICommandBuildLocalGraphicsPipelineState;
struct FRHICommandBuildLocalUniformBuffer;
struct FRHICommandDrawIndexedIndirect;
struct FRHICommandDrawIndexedPrimitive;
struct FRHICommandDrawIndexedPrimitiveIndirect;
struct FRHICommandDrawPrimitive;
struct FRHICommandDrawPrimitiveIndirect;
struct FRHICommandMultiDrawPrimitiveIndirect;
struct FRHICommandSetDepthBounds;
struct FRHICommandEndDrawingViewport;
struct FRHICommandEndFrame;
struct FRHICommandEndOcclusionQueryBatch;
struct FRHICommandEndRenderQuery;
struct FRHICommandEndScene;
struct FRHICommandSetBlendFactor;
struct FRHICommandSetBoundShaderState;
struct FRHICommandSetLocalGraphicsPipelineState;
struct FRHICommandSetRasterizerState;
struct FRHICommandSetRenderTargets;
struct FRHICommandSetRenderTargetsAndClear;
struct FRHICommandSetScissorRect;
struct FRHICommandSetStencilRef;
struct FRHICommandSetStereoViewport;
struct FRHICommandSetStreamSource;
struct FRHICommandSetViewport;
struct FRHICommandTransitionTextures;
struct FRHICommandTransitionTexturesPipeline;
struct FRHICommandTransitionTexturesDepth;
struct FRHICommandTransitionTexturesArray;
struct FRHICommandClearRayTracingBindings;
struct FRHICommandRayTraceDispatch;
struct FRHICommandSetRayTracingBindings;

template <typename TRHIShader> struct FRHICommandSetLocalUniformBuffer;

#if WITH_MGPU
void FRHICommandSetGPUMask::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetGPUMask);

	// Update the RHICmdList copy of the current mask
	CmdList.PersistentState.CurrentGPUMask = GPUMask;

	// Apply the new mask to all contexts owned by this command list.
	for (IRHIComputeContext* Context : CmdList.Contexts)
	{
		if (Context)
		{
			Context->RHISetGPUMask(GPUMask);
		}
	}
}

void FRHICommandTransferResources::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransferResources);
	INTERNAL_DECORATOR_COMPUTE(RHITransferResources)(Params);
}

void FRHICommandTransferResourceSignal::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransferResourceSignal);
	INTERNAL_DECORATOR_COMPUTE(RHITransferResourceSignal)(FenceDatas, SrcGPUMask);
}

void FRHICommandTransferResourceWait::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransferResourceWait);
	INTERNAL_DECORATOR_COMPUTE(RHITransferResourceWait)(FenceDatas);
}

void FRHICommandCrossGPUTransfer::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CrossGPUTransfer);
	INTERNAL_DECORATOR_COMPUTE(RHICrossGPUTransfer)(Params, PreTransfer, PostTransfer);
}

void FRHICommandCrossGPUTransferSignal::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CrossGPUTransferSignal);
	INTERNAL_DECORATOR_COMPUTE(RHICrossGPUTransferSignal)(Params, PreTransfer);
}

void FRHICommandCrossGPUTransferWait::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CrossGPUTransferWait);
	INTERNAL_DECORATOR_COMPUTE(RHICrossGPUTransferWait)(SyncPoints);
}

#endif // WITH_MGPU

void FRHICommandSetStencilRef::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStencilRef);
	INTERNAL_DECORATOR(RHISetStencilRef)(StencilRef);
}

template<> RHI_API void FRHICommandSetShaderParameters<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameters);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderParameters)(Shader, ParametersData, Parameters, ResourceParameters, BindlessParameters);
}

template<> RHI_API void FRHICommandSetShaderParameters<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameters);
	INTERNAL_DECORATOR(RHISetShaderParameters)(Shader, ParametersData, Parameters, ResourceParameters, BindlessParameters);
}

template<> RHI_API void FRHICommandSetShaderUnbinds<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUnbinds);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderUnbinds)(Shader, Unbinds);
}

template<> RHI_API void FRHICommandSetShaderUnbinds<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUnbinds);
	INTERNAL_DECORATOR(RHISetShaderUnbinds)(Shader, Unbinds);
}

void FRHICommandDrawPrimitive::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawPrimitive);
	INTERNAL_DECORATOR(RHIDrawPrimitive)(BaseVertexIndex, NumPrimitives, NumInstances);
}

void FRHICommandDrawIndexedPrimitive::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedPrimitive);
	INTERNAL_DECORATOR(RHIDrawIndexedPrimitive)(IndexBuffer, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
}

void FRHICommandSetBlendFactor::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetBlendFactor);
	INTERNAL_DECORATOR(RHISetBlendFactor)(BlendFactor);
}

void FRHICommandSetStreamSource::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStreamSource);
	INTERNAL_DECORATOR(RHISetStreamSource)(StreamIndex, VertexBuffer, Offset);
}

void FRHICommandSetViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetViewport);
	INTERNAL_DECORATOR(RHISetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FRHICommandSetStereoViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStereoViewport);
	INTERNAL_DECORATOR(RHISetStereoViewport)(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
}

void FRHICommandSetScissorRect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetScissorRect);
	INTERNAL_DECORATOR(RHISetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
}

void FRHICommandBeginRenderPass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginRenderPass);
	INTERNAL_DECORATOR(RHIBeginRenderPass)(Info, Name);
}

void FRHICommandEndRenderPass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndRenderPass);
	INTERNAL_DECORATOR(RHIEndRenderPass)();
}

void FRHICommandNextSubpass::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(NextSubpass);
	INTERNAL_DECORATOR(RHINextSubpass)();
}

void FRHICommandSetComputePipelineState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetComputePipelineState);
	extern RHI_API FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState);
	FRHIComputePipelineState* RHIComputePipelineState = ExecuteSetComputePipelineState(ComputePipelineState);
	INTERNAL_DECORATOR_COMPUTE(RHISetComputePipelineState)(RHIComputePipelineState);
}

void FRHICommandSetGraphicsPipelineState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetGraphicsPipelineState);
	extern FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(FGraphicsPipelineState* GraphicsPipelineState);
	FRHIGraphicsPipelineState* RHIGraphicsPipelineState = ExecuteSetGraphicsPipelineState(GraphicsPipelineState);
	INTERNAL_DECORATOR(RHISetGraphicsPipelineState)(RHIGraphicsPipelineState, StencilRef, bApplyAdditionalState);
}

#if PLATFORM_USE_FALLBACK_PSO
void FRHICommandSetGraphicsPipelineStateFromInitializer::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetGraphicsPipelineStateFromInitializer);
	INTERNAL_DECORATOR(RHISetGraphicsPipelineState)(PsoInit, StencilRef, bApplyAdditionalState);
}
#endif

void FRHICommandDispatchComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchComputeShader);
	INTERNAL_DECORATOR_COMPUTE(RHIDispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void FRHICommandDispatchIndirectComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchIndirectComputeShader);
	INTERNAL_DECORATOR_COMPUTE(RHIDispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
}

void FRHICommandDispatchShaderBundle::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchShaderBundle);
	extern RHI_API FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState);
	for (int32 DispatchIndex = 0; DispatchIndex < Dispatches.Num(); ++DispatchIndex)
	{
		FRHIShaderBundleDispatch& Dispatch = Dispatches[DispatchIndex];
		if (Dispatch.RecordIndex != ~uint32(0u))
		{
			Dispatch.RHIPipeline = ExecuteSetComputePipelineState(Dispatch.PipelineState);
		}
	}
	INTERNAL_DECORATOR_COMPUTE(RHIDispatchShaderBundle)(ShaderBundle, RecordArgBufferSRV, Dispatches, bEmulated);
}

void FRHICommandSetShaderRootConstants::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderRootConstants);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderRootConstants)(Constants);
}

void FRHICommandBeginUAVOverlap::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginUAVOverlap);
	INTERNAL_DECORATOR_COMPUTE(RHIBeginUAVOverlap)();
}

void FRHICommandEndUAVOverlap::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndUAVOverlap);
	INTERNAL_DECORATOR_COMPUTE(RHIEndUAVOverlap)();
}

void FRHICommandBeginSpecificUAVOverlap::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginSpecificUAVOverlap);
	INTERNAL_DECORATOR_COMPUTE(RHIBeginUAVOverlap)(UAVs);
}

void FRHICommandEndSpecificUAVOverlap::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndSpecificUAVOverlap);
	INTERNAL_DECORATOR_COMPUTE(RHIEndUAVOverlap)(UAVs);
}

void FRHICommandDrawPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawPrimitiveIndirect);
	INTERNAL_DECORATOR(RHIDrawPrimitiveIndirect)(ArgumentBuffer, ArgumentOffset);
}

void FRHICommandDrawIndexedIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedIndirect);
	INTERNAL_DECORATOR(RHIDrawIndexedIndirect)(IndexBufferRHI, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
}

void FRHICommandDrawIndexedPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedPrimitiveIndirect);
	INTERNAL_DECORATOR(RHIDrawIndexedPrimitiveIndirect)(IndexBuffer, ArgumentsBuffer, ArgumentOffset);
}

void FRHICommandMultiDrawIndexedPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(MultiDrawIndexedPrimitiveIndirect);
	INTERNAL_DECORATOR(RHIMultiDrawIndexedPrimitiveIndirect)(IndexBuffer, ArgumentBuffer, ArgumentOffset, CountBuffer, CountBufferOffset, MaxDrawArguments);
}

void FRHICommandDispatchMeshShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchMeshShader);
	INTERNAL_DECORATOR(RHIDispatchMeshShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void FRHICommandDispatchIndirectMeshShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchIndirectMeshShader);
	INTERNAL_DECORATOR(RHIDispatchIndirectMeshShader)(ArgumentBuffer, ArgumentOffset);
}

void FRHICommandSetShadingRate::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShadingRate);
	INTERNAL_DECORATOR(RHISetShadingRate)(ShadingRate, Combiner);
}

void FRHICommandSetDepthBounds::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EnableDepthBoundsTest);
	INTERNAL_DECORATOR(RHISetDepthBounds)(MinDepth, MaxDepth);
}

void FRHICommandClearUAVFloat::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ClearUAV);
	INTERNAL_DECORATOR_COMPUTE(RHIClearUAVFloat)(UnorderedAccessViewRHI, Values);
}

void FRHICommandClearUAVUint::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ClearUAV);
	INTERNAL_DECORATOR_COMPUTE(RHIClearUAVUint)(UnorderedAccessViewRHI, Values);
}

void FRHICommandCopyTexture::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CopyTexture);
	INTERNAL_DECORATOR(RHICopyTexture)(SourceTexture, DestTexture, CopyInfo);
}

void FRHICommandResummarizeHTile::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ResummarizeHTile);
	INTERNAL_DECORATOR(RHIResummarizeHTile)(DepthTexture);
}

void FRHICommandBeginTransitions::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginTransitions);
	INTERNAL_DECORATOR_COMPUTE(RHIBeginTransitions)(Transitions);

	for (const FRHITransition* Transition : Transitions)
	{
		Transition->MarkBegin(CmdList.GetPipeline());
	}
}

void FRHICommandEndTransitions::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndTransitions);
	INTERNAL_DECORATOR_COMPUTE(RHIEndTransitions)(Transitions);

	for (const FRHITransition* Transition : Transitions)
	{
		Transition->MarkEnd(CmdList.GetPipeline());
	}
}

void FRHICommandResourceTransition::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ResourceTransition);

	INTERNAL_DECORATOR_COMPUTE(RHIBeginTransitions)(MakeArrayView((const FRHITransition**)&Transition, 1));
	INTERNAL_DECORATOR_COMPUTE(RHIEndTransitions)(MakeArrayView((const FRHITransition**)&Transition, 1));

	// Manual release. No need to free, the instance was allocated on the RHICmdList stack.
	GDynamicRHI->RHIReleaseTransition(Transition);
	Transition->~FRHITransition();
}

void FRHICommandSetTrackedAccess::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetTrackedAccess);
	for (const FRHITrackedAccessInfo& Info : Infos)
	{
		INTERNAL_DECORATOR_COMPUTE(SetTrackedAccess)(Info);
	}
}

void FRHICommandSetAsyncComputeBudget::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetAsyncComputeBudget);
	INTERNAL_DECORATOR_COMPUTE(RHISetAsyncComputeBudget)(Budget);
}

void FRHICommandCopyToStagingBuffer::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EnqueueStagedRead);
	INTERNAL_DECORATOR_COMPUTE(RHICopyToStagingBuffer)(SourceBuffer, DestinationStagingBuffer, Offset, NumBytes);
}

void FRHICommandWriteGPUFence::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(WriteGPUFence);
	INTERNAL_DECORATOR_COMPUTE(RHIWriteGPUFence)(Fence);
	if (Fence)
	{
		Fence->NumPendingWriteCommands.Decrement();
	}
}

void FRHICommandSetStaticUniformBuffers::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStaticUniformBuffers);
	INTERNAL_DECORATOR_COMPUTE(RHISetStaticUniformBuffers)(UniformBuffers);
}

void FRHICommandSetStaticUniformBuffer::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStaticUniformBuffer);
	INTERNAL_DECORATOR_COMPUTE(RHISetStaticUniformBuffer)(Slot, Buffer);
}

void FRHICommandSetUniformBufferDynamicOffset::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUniformBufferDynamicOffset);
	INTERNAL_DECORATOR(RHISetUniformBufferDynamicOffset)(Slot, Offset);
}

void FRHICommandBeginRenderQuery::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginRenderQuery);
	INTERNAL_DECORATOR(RHIBeginRenderQuery)(RenderQuery);
}

void FRHICommandEndRenderQuery::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndRenderQuery);
	INTERNAL_DECORATOR(RHIEndRenderQuery)(RenderQuery);
}

void FRHICommandCalibrateTimers::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CalibrateTimers);
	INTERNAL_DECORATOR(RHICalibrateTimers)(CalibrationQuery);
}

void FRHICommandSubmitCommandsHint::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SubmitCommandsHint);
	INTERNAL_DECORATOR_COMPUTE(RHISubmitCommandsHint)();
}

void FRHICommandPostExternalCommandsReset::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PostExternalCommandsReset);
	INTERNAL_DECORATOR(RHIPostExternalCommandsReset)();
}

void FRHICommandPollOcclusionQueries::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PollOcclusionQueries);
	INTERNAL_DECORATOR(RHIPollOcclusionQueries)();
}

void FRHICommandCopyBufferRegion::Execute(FRHICommandListBase& CmdList)
{
	INTERNAL_DECORATOR(RHICopyBufferRegion)(DestBuffer, DstOffset, SourceBuffer, SrcOffset, NumBytes);
}

#if RHI_RAYTRACING

void FRHICommandBindAccelerationStructureMemory::Execute(FRHICommandListBase& CmdList)
{
	INTERNAL_DECORATOR_COMPUTE(RHIBindAccelerationStructureMemory)(Scene, Buffer, BufferOffset);
}

void FRHICommandBuildAccelerationStructure::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BuildAccelerationStructure);
	INTERNAL_DECORATOR_COMPUTE(RHIBuildAccelerationStructure)(SceneBuildParams);
}

void FRHICommandClearRayTracingBindings::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ClearRayTracingBindings);
	INTERNAL_DECORATOR(RHIClearRayTracingBindings)(Scene);
}

void FRHICommandBuildAccelerationStructures::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BuildAccelerationStructure);
	INTERNAL_DECORATOR_COMPUTE(RHIBuildAccelerationStructures)(Params, ScratchBufferRange);
}

void FRHICommandRayTraceDispatch::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(RayTraceDispatch);
	extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState*);
	if (ArgumentBuffer)
	{
		INTERNAL_DECORATOR(RHIRayTraceDispatchIndirect)(GetRHIRayTracingPipelineState(Pipeline), RayGenShader, Scene, GlobalResourceBindings, ArgumentBuffer, ArgumentOffset);
	}
	else
	{
		INTERNAL_DECORATOR(RHIRayTraceDispatch)(GetRHIRayTracingPipelineState(Pipeline), RayGenShader, Scene, GlobalResourceBindings, Width, Height);
	}
}

void FRHICommandSetRayTracingBindings::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRayTracingHitGroup);
	extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState*);
	if (NumBindings >= 0)
	{
		check(Bindings != nullptr);
		INTERNAL_DECORATOR(RHISetRayTracingBindings)(Scene, GetRHIRayTracingPipelineState(Pipeline), NumBindings, Bindings, BindingType);
	}
	else if (BindingType == ERayTracingBindingType::HitGroup)
	{
		INTERNAL_DECORATOR(RHISetRayTracingHitGroup)(Scene, InstanceIndex, SegmentIndex, ShaderSlot, GetRHIRayTracingPipelineState(Pipeline), ShaderIndex,
			NumUniformBuffers, UniformBuffers,
			LooseParameterDataSize, LooseParameterData,
			UserData);
	}
	else if (BindingType == ERayTracingBindingType::CallableShader)
	{
		INTERNAL_DECORATOR(RHISetRayTracingCallableShader)(Scene, ShaderSlot, GetRHIRayTracingPipelineState(Pipeline), ShaderIndex, NumUniformBuffers, UniformBuffers, UserData);
	}
	else if(BindingType == ERayTracingBindingType::MissShader)
	{
		INTERNAL_DECORATOR(RHISetRayTracingMissShader)(Scene, ShaderSlot, GetRHIRayTracingPipelineState(Pipeline), ShaderIndex, NumUniformBuffers, UniformBuffers, UserData);
	}
	else
	{
		checkNoEntry();
	}
}

#endif // RHI_RAYTRACING

void FRHIResourceUpdateInfo::ReleaseRefs()
{
	switch (Type)
	{
	case UT_Buffer:
		Buffer.DestBuffer->Release();
		if (Buffer.SrcBuffer)
		{
			Buffer.SrcBuffer->Release();
		}
		break;
	case UT_RayTracingGeometry:
		RayTracingGeometry.DestGeometry->Release();
		if (RayTracingGeometry.SrcGeometry)
		{
			RayTracingGeometry.SrcGeometry->Release();
		}
	default:
		// Unrecognized type, do nothing
		break;
	}
}

FRHICommandUpdateRHIResources::~FRHICommandUpdateRHIResources()
{
	if (bNeedReleaseRefs)
	{
		for (int32 Idx = 0; Idx < Num; ++Idx)
		{
			UpdateInfos[Idx].ReleaseRefs();
		}
	}
}

void FRHICommandUpdateRHIResources::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(UpdateRHIResources);
	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		FRHIResourceUpdateInfo& Info = UpdateInfos[Idx];
		switch (Info.Type)
		{
		case FRHIResourceUpdateInfo::UT_Buffer:
			GDynamicRHI->RHITransferBufferUnderlyingResource(
				CmdList,
				Info.Buffer.DestBuffer,
				Info.Buffer.SrcBuffer);
			break;
#if RHI_RAYTRACING
		case FRHIResourceUpdateInfo::UT_RayTracingGeometry:
			GDynamicRHI->RHITransferRayTracingGeometryUnderlyingResource(
				CmdList,
				Info.RayTracingGeometry.DestGeometry,
				Info.RayTracingGeometry.SrcGeometry);
			break;
#endif // RHI_RAYTRACING
		default:
			// Unrecognized type, do nothing
			break;
		}
	}
}

void FRHICommandBeginScene::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginScene);
	INTERNAL_DECORATOR(RHIBeginScene)();
}

void FRHICommandEndScene::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndScene);
	INTERNAL_DECORATOR(RHIEndScene)();
}

void FRHICommandBeginFrame::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginFrame);
	CmdList.GetAsImmediate().ProcessStats();
	INTERNAL_DECORATOR(RHIBeginFrame)();
}

void FRHICommandEndFrame::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndFrame);
	INTERNAL_DECORATOR(RHIEndFrame)();
}

void FRHICommandBeginDrawingViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginDrawingViewport);
	INTERNAL_DECORATOR(RHIBeginDrawingViewport)(Viewport, RenderTargetRHI);
}

void FRHICommandEndDrawingViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawingViewport);
	INTERNAL_DECORATOR(RHIEndDrawingViewport)(Viewport, bPresent, bLockToVsync);
}

void FRHICommandPushEvent::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PushEvent);
	INTERNAL_DECORATOR_COMPUTE(RHIPushEvent)(Name, Color);
}

void FRHICommandPopEvent::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PopEvent);
	INTERNAL_DECORATOR_COMPUTE(RHIPopEvent)();
}

#if RHI_WANT_BREADCRUMB_EVENTS
void FRHICommandSetBreadcrumbStackTop::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(RHISetBreadcrumbStackTop);
	CmdList.Breadcrumbs.SetStackTop(Breadcrumb);
}
#endif

void FRHICommandInvalidateCachedState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(RHIInvalidateCachedState);
	INTERNAL_DECORATOR(RHIInvalidateCachedState)();
}

void FRHICommandDiscardRenderTargets::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(RHIDiscardRenderTargets);
	INTERNAL_DECORATOR(RHIDiscardRenderTargets)(Depth, Stencil, ColorBitMask);
}
