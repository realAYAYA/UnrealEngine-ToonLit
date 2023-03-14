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

class FRHICommandListBase;
class IRHIComputeContext;
struct FComputedBSS;
struct FComputedGraphicsPipelineState;
struct FComputedUniformBuffer;
struct FMemory;
struct FRHICommandBeginDrawingViewport;
struct FRHICommandBeginFrame;
struct FRHICommandBeginRenderQuery;
struct FRHICommandBeginScene;
struct FRHICommandBuildLocalBoundShaderState;
struct FRHICommandBuildLocalGraphicsPipelineState;
struct FRHICommandBuildLocalUniformBuffer;
struct FRHICommandCopyToResolveTarget;
struct FRHICommandDrawIndexedIndirect;
struct FRHICommandDrawIndexedPrimitive;
struct FRHICommandDrawIndexedPrimitiveIndirect;
struct FRHICommandDrawPrimitive;
struct FRHICommandDrawPrimitiveIndirect;
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
struct FRHICommandRayTraceOcclusion;
struct FRHICommandRayTraceIntersection;
struct FRHICommandRayTraceDispatch;
struct FRHICommandSetRayTracingBindings;

template <typename TRHIShader> struct FRHICommandSetLocalUniformBuffer;

void FRHICommandBeginUpdateMultiFrameResource::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginUpdateMultiFrameResource);
	INTERNAL_DECORATOR(RHIBeginUpdateMultiFrameResource)(Texture);
}

void FRHICommandEndUpdateMultiFrameResource::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndUpdateMultiFrameResource);
	INTERNAL_DECORATOR(RHIEndUpdateMultiFrameResource)(Texture);
}

void FRHICommandBeginUpdateMultiFrameUAV::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginUpdateMultiFrameUAV);
	INTERNAL_DECORATOR(RHIBeginUpdateMultiFrameResource)(UAV);
}

void FRHICommandEndUpdateMultiFrameUAV::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndUpdateMultiFrameUAV);
	INTERNAL_DECORATOR(RHIEndUpdateMultiFrameResource)(UAV);
}

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
void FRHICommandWaitForTemporalEffect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(WaitForTemporalEffect);
	INTERNAL_DECORATOR(RHIWaitForTemporalEffect)(EffectName);
}

template<> RHI_API void FRHICommandBroadcastTemporalEffect<FRHITexture>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BroadcastTemporalEffect);
	INTERNAL_DECORATOR(RHIBroadcastTemporalEffect)(EffectName, Resources);
}
template<> RHI_API void FRHICommandBroadcastTemporalEffect<FRHIBuffer>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BroadcastTemporalEffect);
	INTERNAL_DECORATOR(RHIBroadcastTemporalEffect)(EffectName, Resources);
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

#endif // WITH_MGPU

void FRHICommandSetStencilRef::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStencilRef);
	INTERNAL_DECORATOR(RHISetStencilRef)(StencilRef);
}

template<> RHI_API void FRHICommandSetShaderParameter<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameter);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
}

template<> RHI_API void FRHICommandSetShaderParameter<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameter);
	INTERNAL_DECORATOR(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
}

template<> RHI_API void FRHICommandSetShaderUniformBuffer<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUniformBuffer);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
}

template<> RHI_API void FRHICommandSetShaderUniformBuffer<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUniformBuffer);
	INTERNAL_DECORATOR(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
}

template<> RHI_API void FRHICommandSetShaderTexture<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderTexture);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderTexture)(Shader, TextureIndex, Texture);
}

template<> RHI_API void FRHICommandSetShaderTexture<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderTexture);
	INTERNAL_DECORATOR(RHISetShaderTexture)(Shader, TextureIndex, Texture);
}

template<> RHI_API void FRHICommandSetShaderResourceViewParameter<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderResourceViewParameter);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
}

template<> RHI_API void FRHICommandSetShaderResourceViewParameter<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderResourceViewParameter);
	INTERNAL_DECORATOR(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
}

template<> RHI_API void FRHICommandSetUAVParameter<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR_COMPUTE(RHISetUAVParameter)(Shader, UAVIndex, UAV);
}

template<> RHI_API void FRHICommandSetUAVParameter<FRHIPixelShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR(RHISetUAVParameter)(Shader, UAVIndex, UAV);
}

void FRHICommandSetUAVParameter_InitialCount::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR_COMPUTE(RHISetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
}

template<> RHI_API void FRHICommandSetShaderSampler<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderSampler);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderSampler)(Shader, SamplerIndex, Sampler);
}

template<> RHI_API void FRHICommandSetShaderSampler<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderSampler);
	INTERNAL_DECORATOR(RHISetShaderSampler)(Shader, SamplerIndex, Sampler);
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

void FRHICommandSetComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetComputeShader);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	INTERNAL_DECORATOR_COMPUTE(RHISetComputeShader)(ComputeShader);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

void FRHICommandCopyToResolveTarget::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CopyToResolveTarget);
	INTERNAL_DECORATOR(RHICopyToResolveTarget)(SourceTexture, DestTexture, ResolveParams);
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

void FRHICommandBuildLocalUniformBuffer::Execute(FRHICommandListBase& CmdList)
{
	LLM_SCOPE(ELLMTag::Shaders);
	RHISTAT(BuildLocalUniformBuffer);
	check(!IsValidRef(WorkArea.ComputedUniformBuffer->UniformBuffer)); // should not already have been created
	check(WorkArea.Layout);
	check(WorkArea.Contents); 
	if (WorkArea.ComputedUniformBuffer->UseCount)
	{
		WorkArea.ComputedUniformBuffer->UniformBuffer = GDynamicRHI->RHICreateUniformBuffer(WorkArea.Contents, WorkArea.Layout, UniformBuffer_SingleFrame, EUniformBufferValidation::ValidateResources);
	}
	WorkArea.Layout = nullptr;
	WorkArea.Contents = nullptr;
}

template <typename TRHIShader>
void FRHICommandSetLocalUniformBuffer<TRHIShader>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetLocalUniformBuffer);
	check(LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount > 0 && IsValidRef(LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UniformBuffer)); // this should have been created and should have uses outstanding
	INTERNAL_DECORATOR(RHISetShaderUniformBuffer)(Shader, BaseIndex, LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UniformBuffer);
	if (--LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount == 0)
	{
		LocalUniformBuffer.WorkArea->ComputedUniformBuffer->~FComputedUniformBuffer();
	}
}

template struct FRHICommandSetLocalUniformBuffer<FRHIVertexShader>;
template struct FRHICommandSetLocalUniformBuffer<FRHIGeometryShader>;
template struct FRHICommandSetLocalUniformBuffer<FRHIPixelShader>;
template struct FRHICommandSetLocalUniformBuffer<FRHIComputeShader>;
template struct FRHICommandSetLocalUniformBuffer<FRHIMeshShader>;

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

void FRHICommandRayTraceOcclusion::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(RayTraceOcclusion);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	INTERNAL_DECORATOR(RHIRayTraceOcclusion)(Scene, Rays, Output, NumRays);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FRHICommandRayTraceIntersection::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(RayTraceIntersection);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	INTERNAL_DECORATOR(RHIRayTraceIntersection)(Scene, Rays, Output, NumRays);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
	case UT_BufferSRV:
	case UT_BufferFormatSRV:
		BufferSRV.SRV->Release();
		if (BufferSRV.Buffer)
		{
			BufferSRV.Buffer->Release();
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
				Info.Buffer.DestBuffer,
				Info.Buffer.SrcBuffer);
			break;
		case FRHIResourceUpdateInfo::UT_BufferFormatSRV:
			GDynamicRHI->RHIUpdateShaderResourceView(
				Info.BufferSRV.SRV,
				Info.BufferSRV.Buffer,
				Info.BufferSRV.Stride,
				Info.BufferSRV.Format);
			break;
		case FRHIResourceUpdateInfo::UT_BufferSRV:
			GDynamicRHI->RHIUpdateShaderResourceView(
				Info.BufferSRV.SRV,
				Info.BufferSRV.Buffer);
			break;
#if RHI_RAYTRACING
		case FRHIResourceUpdateInfo::UT_RayTracingGeometry:
			GDynamicRHI->RHITransferRayTracingGeometryUnderlyingResource(
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
	RHIPrivateBeginFrame();
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
