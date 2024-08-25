// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRenderPass.cpp: Metal command pass wrapper.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "MetalShaderTypes.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalVertexDeclaration.h"
#include "MetalRenderPass.h"
#include "MetalCommandBuffer.h"
#include "MetalProfiler.h"
#include "MetalFrameAllocator.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHIUtilities.h"

#pragma mark - Private Console Variables -

static int32 GMetalCommandBufferCommitThreshold = 0;
static FAutoConsoleVariableRef CVarMetalCommandBufferCommitThreshold(
	TEXT("rhi.Metal.CommandBufferCommitThreshold"),
	GMetalCommandBufferCommitThreshold,
	TEXT("When enabled (> 0) if the command buffer has more than this number of draw/dispatch command encoded then it will be committed at the next encoder boundary to keep the GPU busy. (Default: 0, set to <= 0 to disable)"));

static int32 GMetalDeferRenderPasses = 1;
static FAutoConsoleVariableRef CVarMetalDeferRenderPasses(
	TEXT("rhi.Metal.DeferRenderPasses"),
	GMetalDeferRenderPasses,
	TEXT("Whether to defer creating render command encoders. (Default: 1)"));

#pragma mark - Public C++ Boilerplate -

FMetalRenderPass::FMetalRenderPass(FMetalCommandList& InCmdList, FMetalStateCache& Cache)
	: CmdList(InCmdList)
	, State(Cache)
	, CurrentEncoder(InCmdList, EMetalCommandEncoderCurrent)
	, RenderPassDesc(nullptr)
	, ComputeDispatchType(MTL::DispatchTypeSerial)
	, NumOutstandingOps(0)
	, bWithinRenderPass(false)
{
}

FMetalRenderPass::~FMetalRenderPass(void)
{
	check(!CurrentEncoder.GetCommandBuffer());
    RenderPassDesc->release();
}

void FMetalRenderPass::SetDispatchType(MTL::DispatchType Type)
{
	ComputeDispatchType = Type;
}

void FMetalRenderPass::Begin()
{
	if (!CurrentEncoder.GetCommandBuffer())
	{
		CurrentEncoder.StartCommandBuffer();
		check(CurrentEncoder.GetCommandBuffer());
	}
}

TRefCountPtr<FMetalFence> const& FMetalRenderPass::Submit(EMetalSubmitFlags Flags)
{
    // Must be on the render thread if there's no RHI thread
    // Must be on the RHI thread otherwise
    CheckMetalThread();
    
    if (Flags & EMetalSubmitFlagsLastCommandBuffer)
    {
        check(CurrentEncoder.GetCommandBuffer());
        FMetalCommandBuffer* CommandBuffer = CurrentEncoder.GetCommandBuffer();
        
        FMetalDeviceContext& DeviceContext = (FMetalDeviceContext&)GetMetalDeviceContext();
        FMetalFrameAllocator* UniformAllocator = DeviceContext.GetUniformAllocator();
        
        UniformAllocator->MarkEndOfFrame(DeviceContext.GetFrameNumberRHIThread(), CommandBuffer);
		
		FMetalFrameAllocator* TransferAllocator = DeviceContext.GetTransferAllocator();
		TransferAllocator->MarkEndOfFrame(DeviceContext.GetFrameNumberRHIThread(), CommandBuffer);
    }
    
    if (CurrentEncoder.GetCommandBuffer() && !(Flags & EMetalSubmitFlagsAsyncCommandBuffer))
    {
        if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
        {
            if (CurrentEncoder.IsRenderCommandEncoderActive())
            {
                State.SetRenderStoreActions(CurrentEncoder, (Flags & EMetalSubmitFlagsBreakCommandBuffer));
				State.FlushVisibilityResults(CurrentEncoder);
            }
            CurrentEncoderFence = CurrentEncoder.EndEncoding();
        }
		
        CurrentEncoder.CommitCommandBuffer(Flags);
    }
	
#if PLATFORM_VISIONOS
	if (CompositorServicesFrame)
	{
		cp_frame_end_submission(CompositorServicesFrame);
		CompositorServicesFrame = nullptr;
	}
#endif

	OutstandingBufferUploads.Empty();
	if (Flags & EMetalSubmitFlagsResetState)
	{
		CurrentEncoder.Reset();
	}
	
	return CurrentEncoderFence;
}

void FMetalRenderPass::BeginRenderPass(MTL::RenderPassDescriptor* RenderPass)
{
	check(!bWithinRenderPass);
	check(!RenderPassDesc);
	check(RenderPass);
	check(!CurrentEncoder.IsRenderCommandEncoderActive());
	check(CurrentEncoder.GetCommandBuffer());
	
	// EndEncoding should provide the encoder fence...
	if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
	{
		State.FlushVisibilityResults(CurrentEncoder);
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	State.SetStateDirty();
	State.SetRenderTargetsActive(true);
	
	RenderPassDesc = RenderPass;
	
	CurrentEncoder.SetRenderPassDescriptor(RenderPassDesc);

	if (!GMetalDeferRenderPasses || !State.CanRestartRenderPass())
	{
		CurrentEncoder.BeginRenderCommandEncoding();
		if (CurrentEncoderFence)
		{
			CurrentEncoder.WaitForFence(CurrentEncoderFence);
			CurrentEncoderFence = nullptr;
		}
		State.SetRenderStoreActions(CurrentEncoder, false);
		check(CurrentEncoder.IsRenderCommandEncoderActive());
	}
	
	bWithinRenderPass = true;
}

static uint32_t MAX_COLOR_RENDER_TARGETS_PER_DESC = 8;

void FMetalRenderPass::RestartRenderPass(MTL::RenderPassDescriptor* RenderPass)
{
	check(bWithinRenderPass);
	check(RenderPassDesc);
	check(CurrentEncoder.GetCommandBuffer());
	
    MTL::RenderPassDescriptor* StartDesc = nullptr;
	if (RenderPass != nullptr)
	{
		// Just restart with the render pass we were given - the caller should have ensured that this is restartable
		check(State.CanRestartRenderPass());
		StartDesc = RenderPass;
	}
	else if (State.PrepareToRestart(CurrentEncoder.IsRenderPassDescriptorValid() && (State.GetRenderPassDescriptor() == CurrentEncoder.GetRenderPassDescriptor())))
	{
		// Restart with the render pass we have in the state cache - the state cache says its safe
		StartDesc = State.GetRenderPassDescriptor();
	}
	else
	{
		
		METAL_FATAL_ERROR(TEXT("Failed to restart render pass with descriptor: %s"), *NSStringToFString(RenderPassDesc->description()));
	}
	check(StartDesc);
	
	RenderPassDesc = StartDesc;
	
#if METAL_DEBUG_OPTIONS
	if ((GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation))
	{
		bool bAllLoadActionsOK = true;
		MTL::RenderPassColorAttachmentDescriptorArray* Attachments = RenderPassDesc->colorAttachments();
		for(uint i = 0; i < MAX_COLOR_RENDER_TARGETS_PER_DESC; i++)
		{
			MTL::RenderPassColorAttachmentDescriptor* Desc = Attachments->object(i);
			if(Desc && Desc->texture())
			{
				bAllLoadActionsOK &= (Desc->loadAction() != MTL::LoadActionClear);
			}
		}
		if(RenderPassDesc->depthAttachment() && RenderPassDesc->depthAttachment()->texture())
		{
			bAllLoadActionsOK &= (RenderPassDesc->depthAttachment()->loadAction() != MTL::LoadActionClear);
		}
		if(RenderPassDesc->stencilAttachment() && RenderPassDesc->stencilAttachment()->texture())
		{
			bAllLoadActionsOK &= (RenderPassDesc->stencilAttachment()->loadAction() != MTL::LoadActionClear);
		}
		
		if (!bAllLoadActionsOK)
		{
			UE_LOG(LogMetal, Warning, TEXT("Tried to restart render encoding with a clear operation - this would erroneously re-clear any existing draw calls: %s"), *NSStringToFString(RenderPassDesc->description()));
			
			for(uint i = 0; i < MAX_COLOR_RENDER_TARGETS_PER_DESC; i++)
			{
				MTL::RenderPassColorAttachmentDescriptor* Desc = Attachments->object(i);
				if(Desc && Desc->texture())
				{
					Desc->setLoadAction(MTL::LoadActionLoad);
				}
			}
			if(RenderPassDesc->depthAttachment() && RenderPassDesc->depthAttachment()->texture())
			{
				RenderPassDesc->depthAttachment()->setLoadAction(MTL::LoadActionLoad);
			}
			if(RenderPassDesc->stencilAttachment() && RenderPassDesc->stencilAttachment()->texture())
			{
				RenderPassDesc->stencilAttachment()->setLoadAction(MTL::LoadActionLoad);
			}
		}
	}
#endif
	
	// EndEncoding should provide the encoder fence...
	if (CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive() || CurrentEncoder.IsRenderCommandEncoderActive())
	{
		if (CurrentEncoder.IsRenderCommandEncoderActive())
		{
			State.SetRenderStoreActions(CurrentEncoder, true);
			State.FlushVisibilityResults(CurrentEncoder);
		}
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	State.SetStateDirty();
	State.SetRenderTargetsActive(true);
	
	CurrentEncoder.SetRenderPassDescriptor(RenderPassDesc);
	CurrentEncoder.BeginRenderCommandEncoding();
	if (CurrentEncoderFence)
	{
		CurrentEncoder.WaitForFence(CurrentEncoderFence);
		CurrentEncoderFence = nullptr;
	}
	State.SetRenderStoreActions(CurrentEncoder, false);
	
	check(CurrentEncoder.IsRenderCommandEncoderActive());
}

#if METAL_USE_METAL_SHADER_CONVERTER
inline void IRBindBytesToEncoder(MTL::RenderCommandEncoder* Encoder, const IRRuntimeDrawParams& DrawArgs, const IRRuntimeDrawInfo& DrawInfos)
{
    Encoder->setVertexBytes(&DrawArgs, sizeof(IRRuntimeDrawParams), kIRArgumentBufferDrawArgumentsBindPoint);
	Encoder->setVertexBytes(&DrawInfos, sizeof(IRRuntimeDrawInfo), kIRArgumentBufferUniformsBindPoint);
}

static void IRBindIndexedDrawArguments(MTL::RenderCommandEncoder* Encoder, MTL::PrimitiveType PrimitiveType, uint32 NumIndices, uint32 NumInstances, uint32 BaseIndexLocation, int32 BaseVertexIndex, uint32 BaseInstanceIndex, const FMetalBufferPtr IndexBuffer, MTL::IndexType IndexType, FMetalStateCache& State)
{
    IRRuntimeDrawParams DrawParams;
    IRRuntimeDrawIndexedArgument& DrawArgs = DrawParams.drawIndexed;
    DrawArgs = { 0 };
    DrawArgs.indexCountPerInstance = NumIndices;
    DrawArgs.instanceCount = NumInstances;
    DrawArgs.startIndexLocation = BaseIndexLocation;
    DrawArgs.baseVertexLocation = BaseVertexIndex;
    DrawArgs.startInstanceLocation = BaseInstanceIndex;

    IRRuntimeDrawInfo DrawInfos = { 0 };
    DrawInfos.primitiveTopology = static_cast<uint8_t>(PrimitiveType);
    DrawInfos.indexType = static_cast<uint16_t>(IndexType);
    DrawInfos.indexBuffer = IndexBuffer->GetGPUAddress();

    // TODO: Could we improve this? (e.g. cache the mapped resources to avoid blindly remapping the index buffer?)
    Encoder->useResource(IndexBuffer->GetMTLBuffer().get(), MTL::ResourceUsageRead);

    IRBindBytesToEncoder(Encoder, DrawParams, DrawInfos);
    State.IRMapVertexBuffers(Encoder);
}

static void IRBindDrawArguments(MTL::RenderCommandEncoder* Encoder, MTL::PrimitiveType PrimitiveType, uint32 NumVertices, uint32 NumInstances, uint32 BaseVertexIndex, uint32 BaseInstanceIndex, FMetalStateCache& State)
{
    IRRuntimeDrawParams DrawParams;
    IRRuntimeDrawArgument& DrawArgs = DrawParams.draw;
    DrawArgs = { 0 };
    DrawArgs.vertexCountPerInstance = NumVertices;
    DrawArgs.instanceCount = NumInstances;
    DrawArgs.startVertexLocation = BaseVertexIndex;
    DrawArgs.startInstanceLocation = BaseInstanceIndex;

    IRRuntimeDrawInfo DrawInfos = { 0 };
    DrawInfos.primitiveTopology = static_cast<uint8_t>(PrimitiveType);

    IRBindBytesToEncoder(Encoder, DrawParams, DrawInfos);
    State.IRMapVertexBuffers(Encoder);
}

static void IRBindIndirectDrawArguments(MTL::RenderCommandEncoder* Encoder, MTL::PrimitiveType PrimitiveType, FMetalBufferPtr TheBackingBuffer, const uint32 ArgumentOffset, FMetalStateCache& State)
{
    IRRuntimeDrawInfo DrawInfos = { 0 };
    DrawInfos.primitiveTopology = static_cast<uint8_t>(PrimitiveType);
    
    Encoder->useResource(TheBackingBuffer->GetMTLBuffer().get(), MTL::ResourceUsageRead);
    
    Encoder->setVertexBuffer(TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + ArgumentOffset, kIRArgumentBufferDrawArgumentsBindPoint);
    Encoder->setVertexBytes(&DrawInfos, sizeof(IRRuntimeDrawInfo), kIRArgumentBufferUniformsBindPoint);
    
    State.IRMapVertexBuffers(Encoder);
}

static void IRBindIndirectIndexedDrawArguments(MTL::RenderCommandEncoder* Encoder, MTL::PrimitiveType PrimitiveType, FMetalBufferPtr TheBackingBuffer, FMetalBufferPtr TheBackingIndexBuffer, MTL::IndexType IndexType, const uint32 ArgumentOffset, FMetalStateCache& State)
{
    IRRuntimeDrawInfo DrawInfos = { 0 };
    DrawInfos.primitiveTopology = static_cast<uint8_t>(PrimitiveType);
    DrawInfos.indexType = static_cast<uint16_t>(IndexType);
    DrawInfos.indexBuffer = TheBackingIndexBuffer->GetGPUAddress();
    
    Encoder->useResource(TheBackingBuffer->GetMTLBuffer().get(), MTL::ResourceUsageRead);
    Encoder->useResource(TheBackingIndexBuffer->GetMTLBuffer().get(), MTL::ResourceUsageRead);
    
    Encoder->setVertexBuffer(TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + ArgumentOffset, kIRArgumentBufferDrawArgumentsBindPoint);
    Encoder->setVertexBytes(&DrawInfos, sizeof(IRRuntimeDrawInfo), kIRArgumentBufferUniformsBindPoint);
    
    State.IRMapVertexBuffers(Encoder);
}

#if PLATFORM_SUPPORTS_MESH_SHADERS
static void IRBindIndirectMeshDrawArguments(MTL::RenderCommandEncoder* Encoder, MTL::PrimitiveType PrimitiveType, FMetalBufferPtr TheBackingBuffer, const uint32 ArgumentOffset, FMetalStateCache& State)
{
    IRRuntimeDrawInfo DrawInfos = { 0 };
    DrawInfos.primitiveTopology = static_cast<uint8_t>(PrimitiveType);
    
    Encoder->useResource(TheBackingBuffer->GetMTLBuffer().get(), MTL::ResourceUsageRead);
    
    Encoder->setMeshBuffer(TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + ArgumentOffset, kIRArgumentBufferDrawArgumentsBindPoint);
    Encoder->setMeshBytes(&DrawInfos, sizeof(IRRuntimeDrawInfo), kIRArgumentBufferUniformsBindPoint);
    
    Encoder->setObjectBuffer(TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + ArgumentOffset, kIRArgumentBufferDrawArgumentsBindPoint);
    Encoder->setObjectBytes(&DrawInfos, sizeof(IRRuntimeDrawInfo), kIRArgumentBufferUniformsBindPoint);
    
    State.IRMapVertexBuffers(Encoder, true);
}

#endif

static IRRuntimeDrawInfo IRRuntimeCalculateDrawInfoForGSEmulation(IRRuntimePrimitiveType primitiveType, uint32 vertexSizeInBytes, uint32 maxInputPrimitivesPerMeshThreadgroup, uint32 instanceCount)
{
    const uint32_t PrimitiveVertexCount = IRRuntimePrimitiveTypeVertexCount(primitiveType);
    const uint32_t Alignment = PrimitiveVertexCount;

    const uint32_t TotalPayloadBytes = 16384;
    const uint32_t PayloadBytesForMetadata = 32;
    const uint32_t PayloadBytesForVertexData = TotalPayloadBytes - PayloadBytesForMetadata;

    const uint32_t MaxVertexCountLimitedByPayloadMemory = (((PayloadBytesForVertexData / vertexSizeInBytes)) / Alignment) * Alignment;

    const uint32_t MaxMeshThreadgroupsPerObjectThreadgroup = 1024;
    const uint32_t MaxPrimCountLimitedByAmplificationRate = MaxMeshThreadgroupsPerObjectThreadgroup * maxInputPrimitivesPerMeshThreadgroup;
    uint32_t MaxPrimsPerObjectThreadgroup = FMath::Min(MaxVertexCountLimitedByPayloadMemory / PrimitiveVertexCount, MaxPrimCountLimitedByAmplificationRate);

    const uint32_t MaxThreadsPerThreadgroup = 256;
    MaxPrimsPerObjectThreadgroup = FMath::Min(MaxPrimsPerObjectThreadgroup, MaxThreadsPerThreadgroup / PrimitiveVertexCount);

    IRRuntimeDrawInfo Infos = {0};
    Infos.primitiveTopology = (uint8)primitiveType;
    Infos.threadsPerPatch = PrimitiveVertexCount;
    Infos.maxInputPrimitivesPerMeshThreadgroup = maxInputPrimitivesPerMeshThreadgroup;
    Infos.objectThreadgroupVertexStride = (uint16)(MaxPrimsPerObjectThreadgroup * PrimitiveVertexCount);
    Infos.meshThreadgroupPrimitiveStride = (uint16)maxInputPrimitivesPerMeshThreadgroup;
    Infos.gsInstanceCount = (uint16)instanceCount;
    Infos.patchesPerObjectThreadgroup = (uint16)MaxPrimsPerObjectThreadgroup;
    Infos.inputControlPointsPerPatch = (uint8)PrimitiveVertexCount;
    
    return Infos;
}
#endif

void FMetalRenderPass::DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	NumInstances = FMath::Max(NumInstances,1u);
	
	ConditionalSwitchToRender();
	check(CurrentEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsRenderCommandEncoderActive());
	
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    if (IsValidRef(State.GetGraphicsPSO()->GeometryShader))
    {
        DispatchMeshShaderGSEmulation(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
        return;
    }
#endif

	PrepareToRender(PrimitiveType);

	// draw!
	// how many verts to render
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

#if METAL_USE_METAL_SHADER_CONVERTER
	if(IsMetalBindlessEnabled())
	{
		IRBindDrawArguments(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), NumVertices, NumInstances, BaseVertexIndex, 0, State);
	}
#endif
    
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeDraw(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__, NumPrimitives, NumVertices, NumInstances));
	CurrentEncoder.GetRenderCommandEncoder()->drawPrimitives(TranslatePrimitiveType(PrimitiveType), BaseVertexIndex, NumVertices, NumInstances);
	
	ConditionalSubmit();	
}

void FMetalRenderPass::DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalRHIBuffer* VertexBuffer, uint32 ArgumentOffset)
{
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		ConditionalSwitchToRender();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		FMetalBufferPtr TheBackingBuffer = VertexBuffer->GetCurrentBuffer();
		check(TheBackingBuffer);
		
		PrepareToRender(PrimitiveType);
		
#if METAL_USE_METAL_SHADER_CONVERTER
		if(IsMetalBindlessEnabled())
		{
			// TODO: Carl - Remove this when API validation is fixed
			// Binding to uniforms bind point to work around error in API validation
			CurrentEncoder.GetRenderCommandEncoder()->setVertexBuffer(TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + ArgumentOffset, kIRArgumentBufferUniformsBindPoint);
			CurrentEncoder.GetRenderCommandEncoder()->setVertexBuffer(TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + ArgumentOffset, kIRArgumentBufferDrawArgumentsBindPoint);
		}
#endif
		
		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeDraw(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__, 1, 1, 1));
		CurrentEncoder.GetRenderCommandEncoder()->drawPrimitives(TranslatePrimitiveType(PrimitiveType),
																 TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + ArgumentOffset);

		ConditionalSubmit();
	}
	else
	{
		NOT_SUPPORTED("RHIDrawPrimitiveIndirect");
	}
}

void FMetalRenderPass::DrawIndexedPrimitive(FMetalBufferPtr IndexBuffer, uint32 IndexStride, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	// We need at least one to cover all use cases
	NumInstances = FMath::Max(NumInstances,1u);
	
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	{
		FMetalGraphicsPipelineState* PipelineState = State.GetGraphicsPSO();
		check(PipelineState != nullptr);
		FMetalVertexDeclaration* VertexDecl = PipelineState->VertexDeclaration;
		check(VertexDecl != nullptr);
		
		// Set our local copy and try to disprove the passed in value
		uint32 ClampedNumInstances = NumInstances;
		const CrossCompiler::FShaderBindingInOutMask& InOutMask = PipelineState->VertexShader->Bindings.InOutMask;

		// I think it is valid to have no elements in this list
		for(int VertexElemIdx = 0;VertexElemIdx < VertexDecl->Elements.Num();++VertexElemIdx)
		{
			FVertexElement const & VertexElem = VertexDecl->Elements[VertexElemIdx];
			if(VertexElem.Stride > 0 && VertexElem.bUseInstanceIndex && InOutMask.IsFieldEnabled(VertexElem.AttributeIndex))
			{
				uint32 AvailElementCount = 0;
				
				uint32 BufferSize = State.GetVertexBufferSize(VertexElem.StreamIndex);
				uint32 ElementCount = (BufferSize / VertexElem.Stride);
				
				if(ElementCount > FirstInstance)
				{
					AvailElementCount = ElementCount - FirstInstance;
				}
				
				ClampedNumInstances = FMath::Clamp<uint32>(ClampedNumInstances, 0, AvailElementCount);
				
				if(ClampedNumInstances < NumInstances)
				{
					// Setting NumInstances to ClampedNumInstances would fix any visual rendering bugs resulting from this bad call but these draw calls are wrong - don't hide the issue
					UE_LOG(LogMetal, Error, TEXT("Metal DrawIndexedPrimitive requested to draw %d Instances but vertex stream only has %d instance data available. ShaderName: %s, Deficient Attribute Index: %u"), NumInstances, ClampedNumInstances,
						   PipelineState->PixelShader->GetShaderName(), VertexElem.AttributeIndex);
				}
			}
		}
	}
#endif
	
	ConditionalSwitchToRender();
	check(CurrentEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsRenderCommandEncoderActive());
	
	PrepareToRender(PrimitiveType);
	
	NS::UInteger NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
								   
#if METAL_USE_METAL_SHADER_CONVERTER
	if(IsMetalBindlessEnabled())
	{
		uint32 BaseIndexLocation = (StartIndex * IndexStride);
		MTL::IndexType IndexType = ((IndexStride == 2) ? MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32);
		
		IRBindIndexedDrawArguments(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), NumIndices, NumInstances, BaseIndexLocation, BaseVertexIndex, FirstInstance, IndexBuffer, IndexType, State);
	}
#endif

	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeDraw(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__, NumPrimitives, NumVertices, NumInstances));
	if (GRHISupportsBaseVertexIndex && GRHISupportsFirstInstance)
	{
		CurrentEncoder.GetRenderCommandEncoder()->drawIndexedPrimitives(TranslatePrimitiveType(PrimitiveType), NumIndices,
                                                                        ((IndexStride == 2) ? MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32),
                                                                        IndexBuffer->GetMTLBuffer().get(), IndexBuffer->GetOffset() + (StartIndex * IndexStride),
                                                                        NumInstances, BaseVertexIndex, FirstInstance);
	}
	else
	{
		CurrentEncoder.GetRenderCommandEncoder()->drawIndexedPrimitives(TranslatePrimitiveType(PrimitiveType), NumIndices,
                                                                        ((IndexStride == 2) ? MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32),
                                                                        IndexBuffer->GetMTLBuffer().get(), IndexBuffer->GetOffset() + (StartIndex * IndexStride),
                                                                        NumInstances);
	}
	
	ConditionalSubmit();
}

void FMetalRenderPass::DrawIndexedIndirect(FMetalRHIBuffer* IndexBuffer, uint32 PrimitiveType, FMetalRHIBuffer* VertexBuffer, int32 DrawArgumentsIndex)
{
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		ConditionalSwitchToRender();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		FMetalBufferPtr TheBackingIndexBuffer = IndexBuffer->GetCurrentBuffer();
		FMetalBufferPtr TheBackingBuffer = VertexBuffer->GetCurrentBuffer();
		
		check(TheBackingIndexBuffer);
		check(TheBackingBuffer);
		
		// finalize any pending state
		PrepareToRender(PrimitiveType);
		
#if METAL_USE_METAL_SHADER_CONVERTER
		if(IsMetalBindlessEnabled())
		{
			IRBindIndirectIndexedDrawArguments(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), TheBackingBuffer, TheBackingIndexBuffer, IndexBuffer->GetIndexType(), (DrawArgumentsIndex * 5 * sizeof(uint32)), State);
		}
#endif

		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeDraw(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__, 1, 1, 1));
		
        CurrentEncoder.GetRenderCommandEncoder()->drawIndexedPrimitives(TranslatePrimitiveType(PrimitiveType),
                                                                        IndexBuffer->GetIndexType(), TheBackingIndexBuffer->GetMTLBuffer().get(), TheBackingIndexBuffer->GetOffset(),
                                                                        TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + (DrawArgumentsIndex * 5 * sizeof(uint32)));

		ConditionalSubmit();
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedIndirect");
	}
}

void FMetalRenderPass::DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalRHIBuffer* IndexBuffer,FMetalRHIBuffer* VertexBuffer,uint32 ArgumentOffset)
{
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{		 
		ConditionalSwitchToRender();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		FMetalBufferPtr TheBackingIndexBuffer = IndexBuffer->GetCurrentBuffer();
		FMetalBufferPtr TheBackingBuffer = VertexBuffer->GetCurrentBuffer();
		
		check(TheBackingIndexBuffer);
		check(TheBackingBuffer);
		
		PrepareToRender(PrimitiveType);
		
#if METAL_USE_METAL_SHADER_CONVERTER
		if(IsMetalBindlessEnabled())
		{
			IRBindIndirectIndexedDrawArguments(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), TheBackingBuffer, TheBackingIndexBuffer, IndexBuffer->GetIndexType(), ArgumentOffset, State);
		}
#endif

		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeDraw(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__, 1, 1, 1));
		CurrentEncoder.GetRenderCommandEncoder()->drawIndexedPrimitives(TranslatePrimitiveType(PrimitiveType), IndexBuffer->GetIndexType(),
                                                                        TheBackingIndexBuffer->GetMTLBuffer().get(), TheBackingIndexBuffer->GetOffset(),
                                                                        TheBackingBuffer->GetMTLBuffer().get(), TheBackingBuffer->GetOffset() + ArgumentOffset);
		
		ConditionalSubmit();
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedPrimitiveIndirect");
	}
}

void FMetalRenderPass::Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	{
		ConditionalSwitchToCompute();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsComputeCommandEncoderActive());

		PrepareToDispatch();
		
		TRefCountPtr<FMetalComputeShader> ComputeShader = State.GetComputeShader();
		check(ComputeShader);
		
		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeDispatch(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
		
        MTL::Size ThreadgroupCounts = MTL::Size(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
		check(ComputeShader->NumThreadsX > 0 && ComputeShader->NumThreadsY > 0 && ComputeShader->NumThreadsZ > 0);
        MTL::Size Threadgroups = MTL::Size(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		CurrentEncoder.GetComputeCommandEncoder()->dispatchThreadgroups(Threadgroups, ThreadgroupCounts);
		
		ConditionalSubmit();
	}
}

void FMetalRenderPass::DispatchIndirect(FMetalRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
	check(ArgumentBuffer);
	{
		ConditionalSwitchToCompute();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsComputeCommandEncoderActive());
		
		PrepareToDispatch();
		
		TRefCountPtr<FMetalComputeShader> ComputeShader = State.GetComputeShader();
		check(ComputeShader);
		
		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeDispatch(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
		MTL::Size ThreadgroupCounts = MTL::Size(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
		check(ComputeShader->NumThreadsX > 0 && ComputeShader->NumThreadsY > 0 && ComputeShader->NumThreadsZ > 0);

		CurrentEncoder.GetComputeCommandEncoder()->dispatchThreadgroups(ArgumentBuffer->GetCurrentBuffer()->GetMTLBuffer().get(),
                                                                        ArgumentBuffer->GetCurrentBuffer()->GetOffset() + ArgumentOffset, ThreadgroupCounts);

		ConditionalSubmit();
	}
}

#if PLATFORM_SUPPORTS_MESH_SHADERS
void FMetalRenderPass::DispatchMeshShader(uint32 PrimitiveType, uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
    checkNoEntry();
}

void FMetalRenderPass::DispatchIndirectMeshShader(uint32 PrimitiveType, FMetalRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
    ConditionalSwitchToRender();
    check(CurrentEncoder.GetCommandBuffer());
    check(CurrentEncoder.IsRenderCommandEncoderActive());
    
    FMetalBufferPtr TheBackingBuffer = ArgumentBuffer->GetCurrentBuffer();
    check(TheBackingBuffer);
    
    PrepareToRender(PrimitiveType);

#if METAL_USE_METAL_SHADER_CONVERTER
	if(IsMetalBindlessEnabled())
	{
		IRBindIndirectMeshDrawArguments(CurrentEncoder.GetRenderCommandEncoder(), TranslatePrimitiveType(PrimitiveType), TheBackingBuffer, ArgumentOffset, State);
	}
#endif
    
    // TODO: Cache this at RHI init time?
    const uint32 MSThreadGroupSize = FDataDrivenShaderPlatformInfo::GetMaxMeshShaderThreadGroupSize(GMaxRHIShaderPlatform);
    CurrentEncoder.GetRenderCommandEncoder()->drawMeshThreadgroups(TheBackingBuffer->GetMTLBuffer().get(),
                                                                ArgumentOffset,
																MTL::Size::Make(MSThreadGroupSize, 1, 1),
																MTL::Size::Make(MSThreadGroupSize, 1, 1));
    
    ConditionalSubmit();
}
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
void FMetalRenderPass::DispatchMeshShaderGSEmulation(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
    ConditionalSwitchToRender();
    check(CurrentEncoder.GetCommandBuffer());
    check(CurrentEncoder.IsRenderCommandEncoderActive());
    
    FMetalVertexShader* VertexShader = (FMetalVertexShader*)State.GetGraphicsPSO()->VertexShader.GetReference();
    FMetalGeometryShader* GeometryShader = (FMetalGeometryShader*)State.GetGraphicsPSO()->GeometryShader.GetReference();
    check(VertexShader);
    check(GeometryShader);
    
    PrepareToRender(PrimitiveType);
    
    IRRuntimeDrawInfo DrawInfos = IRRuntimeCalculateDrawInfoForGSEmulation((IRRuntimePrimitiveType)TranslatePrimitiveType(PrimitiveType),
                                                                          VertexShader->Bindings.OutputSizeVS,
                                                                          GeometryShader->Bindings.MaxInputPrimitivesPerMeshThreadgroupGS,
                                                                          NumInstances);
    
    uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
    
    MTL::Size objectThreadgroupCountTemp = IRRuntimeCalculateObjectTgCountForTessellationAndGeometryEmulation(NumVertices,
														DrawInfos.objectThreadgroupVertexStride,
														(IRRuntimePrimitiveType)TranslatePrimitiveType(PrimitiveType),
														NumInstances);
	MTL::Size objectThreadgroupCount = MTL::Size::Make(objectThreadgroupCountTemp.width,
													   objectThreadgroupCountTemp.height,
													   objectThreadgroupCountTemp.depth);
	
    uint32 ObjectThreadgroupSize = 0;
    uint32 MeshThreadgroupSize = 0;
    
    IRRuntimeCalculateThreadgroupSizeForGeometry((IRRuntimePrimitiveType)TranslatePrimitiveType(PrimitiveType),
                                                 GeometryShader->Bindings.MaxInputPrimitivesPerMeshThreadgroupGS,
                                                 DrawInfos.objectThreadgroupVertexStride,
                                                 &ObjectThreadgroupSize,
                                                 &MeshThreadgroupSize);
    
    IRRuntimeDrawParams DrawParams;
    IRRuntimeDrawArgument& DrawArgs = DrawParams.draw;
    DrawArgs = { 0 };
    DrawArgs.instanceCount = NumInstances;
    DrawArgs.startInstanceLocation = 0;
    DrawArgs.vertexCountPerInstance = NumVertices;
    DrawArgs.startVertexLocation = BaseVertexIndex;
    
    CurrentEncoder.GetRenderCommandEncoder()->setMeshBytes(&DrawParams, sizeof(IRRuntimeDrawParams), 													kIRArgumentBufferDrawArgumentsBindPoint);
    CurrentEncoder.GetRenderCommandEncoder()->setMeshBytes(&DrawInfos, sizeof(IRRuntimeDrawInfo), 													kIRArgumentBufferUniformsBindPoint);
    
    CurrentEncoder.GetRenderCommandEncoder()->setObjectBytes(&DrawParams, sizeof(IRRuntimeDrawParams), 													kIRArgumentBufferDrawArgumentsBindPoint);
    CurrentEncoder.GetRenderCommandEncoder()->setObjectBytes(&DrawInfos, sizeof(IRRuntimeDrawInfo), 													kIRArgumentBufferUniformsBindPoint);
    
    State.IRMapVertexBuffers(CurrentEncoder.GetRenderCommandEncoder(), true);
    
    CurrentEncoder.GetRenderCommandEncoder()->drawMeshThreadgroups(objectThreadgroupCount,
																MTL::Size::Make(ObjectThreadgroupSize, 1, 1),
																MTL::Size::Make(MeshThreadgroupSize, 1, 1));
    
    ConditionalSubmit();
}
#endif


TRefCountPtr<FMetalFence> const& FMetalRenderPass::EndRenderPass(void)
{
	if (bWithinRenderPass)
	{
		check(RenderPassDesc);
		check(CurrentEncoder.GetCommandBuffer());
		
		// This just calls End - it exists only to enforce assumptions
		End();
	}
	return CurrentEncoderFence;
}

void FMetalRenderPass::InsertTextureBarrier()
{
#if PLATFORM_MAC
	check(CurrentEncoder.IsRenderCommandEncoderActive());
	
	MTL::RenderCommandEncoder* RenderEncoder = CurrentEncoder.GetRenderCommandEncoder();
	check(RenderEncoder);
	RenderEncoder->memoryBarrier(MTL::BarrierScopeRenderTargets, MTL::RenderStageFragment, MTL::RenderStageVertex);
#endif
}

void FMetalRenderPass::CopyFromTextureToBuffer(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, FMetalBufferPtr toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, MTL::BlitOption options)
{
	ConditionalSwitchToBlit();
    MTL::BlitCommandEncoder* Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	{
		if(Texture)
		{
			Encoder->copyFromTexture(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize,
									 toBuffer->GetMTLBuffer().get(), destinationOffset + toBuffer->GetOffset(), destinationBytesPerRow, destinationBytesPerImage, options);
		}
	}
	ConditionalSubmit();
}

void FMetalRenderPass::CopyFromBufferToTexture(FMetalBufferPtr Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin, MTL::BlitOption options)
{
	ConditionalSwitchToBlit();
	MTL::BlitCommandEncoder* Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	if (options == MTL::BlitOptionNone)
	{
        //MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(Buffer, sourceOffset, sourceBytesPerRow, sourceBytesPerImage, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin));
        Encoder->copyFromBuffer(Buffer->GetMTLBuffer().get(), sourceOffset + Buffer->GetOffset(), sourceBytesPerRow, sourceBytesPerImage, sourceSize,
                                toTexture, destinationSlice, destinationLevel, destinationOrigin);
	}
	else
	{
		//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(Buffer, sourceOffset, sourceBytesPerRow, sourceBytesPerImage, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin, options));
        Encoder->copyFromBuffer(Buffer->GetMTLBuffer().get(), sourceOffset + Buffer->GetOffset(), sourceBytesPerRow, sourceBytesPerImage, sourceSize,
                                toTexture, destinationSlice, destinationLevel, destinationOrigin, options);
	}
	ConditionalSubmit();
}

void FMetalRenderPass::CopyFromTextureToTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin)
{
	ConditionalSwitchToBlit();
	MTL::BlitCommandEncoder* Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin));
    Encoder->copyFromTexture(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin);
	ConditionalSubmit();
}

void FMetalRenderPass::CopyFromBufferToBuffer(FMetalBufferPtr SourceBuffer, NS::UInteger SourceOffset, FMetalBufferPtr DestinationBuffer, NS::UInteger DestinationOffset, NS::UInteger Size)
{
	ConditionalSwitchToBlit();
	MTL::BlitCommandEncoder* Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(SourceBuffer, SourceOffset, DestinationBuffer, DestinationOffset, Size));
	
    Encoder->copyFromBuffer(SourceBuffer->GetMTLBuffer().get(), SourceOffset + SourceBuffer->GetOffset(),
                            DestinationBuffer->GetMTLBuffer().get(), DestinationOffset + DestinationBuffer->GetOffset(), Size);
	ConditionalSubmit();
}

void FMetalRenderPass::PresentTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin)
{
	ConditionalSwitchToBlit();
	MTL::BlitCommandEncoder* Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin));
    Encoder->copyFromTexture(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin);
}

#if PLATFORM_VISIONOS
void FMetalRenderPass::EncodePresentImmersive(cp_drawable_t Drawable, cp_frame_t Frame)
{
	check(bWithinRenderPass == false); // Call RenderPassEnd before calling this function
	
	FMetalCommandBuffer* CurrentCommandBuffer = GetCurrentCommandBuffer();
	check(CurrentCommandBuffer);
	check(CompositorServicesFrame == nullptr);
	cp_drawable_encode_present(Drawable, (__bridge id<MTLCommandBuffer>)CurrentCommandBuffer->GetMTLCmdBuffer().get());
	CompositorServicesFrame = Frame;
}
#endif

void FMetalRenderPass::SynchronizeTexture(MTL::Texture* Texture, uint32 Slice, uint32 Level)
{
	check(Texture);
#if PLATFORM_MAC
	ConditionalSwitchToBlit();
	MTL::BlitCommandEncoder* Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Synchronize(Texture, Slice, Level));
    Encoder->synchronizeTexture(Texture, Slice, Level);
	ConditionalSubmit();
#endif
}

void FMetalRenderPass::SynchroniseResource(MTL::Resource* Resource)
{
	check(Resource);
#if PLATFORM_MAC
	ConditionalSwitchToBlit();
	MTL::BlitCommandEncoder* Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Synchronize(Resource));
    Encoder->synchronizeResource(Resource);
	ConditionalSubmit();
#endif
}

void FMetalRenderPass::FillBuffer(MTL::Buffer* Buffer, NS::Range Range, uint8 Value)
{
	check(Buffer);
	
	MTL::BlitCommandEncoder *TargetEncoder;
	{
		ConditionalSwitchToBlit();
		TargetEncoder = CurrentEncoder.GetBlitCommandEncoder();
		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), FString::Printf(TEXT("FillBuffer: %p %llu %llu"), Buffer, Range.location, Range.length)));
	}
	
	check(TargetEncoder);
	
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, TargetEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Fill(Buffer, Range, Value));
    TargetEncoder->fillBuffer(Buffer, Range, Value);
	
	{
		ConditionalSubmit();
	}
}

bool FMetalRenderPass::AsyncCopyFromBufferToTexture(FMetalBufferPtr Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin, MTL::BlitOption options)
{
    MTL::BlitCommandEncoder* TargetEncoder;
	{
		ConditionalSwitchToBlit();
		TargetEncoder = CurrentEncoder.GetBlitCommandEncoder();
		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	}
	
	check(TargetEncoder);
	
	if (options == MTL::BlitOptionNone)
	{
		//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, TargetEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(Buffer, sourceOffset, sourceBytesPerRow, sourceBytesPerImage, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin));
        TargetEncoder->copyFromBuffer(Buffer->GetMTLBuffer().get(), sourceOffset + Buffer->GetOffset(), sourceBytesPerRow, sourceBytesPerImage, sourceSize,
                                      toTexture, destinationSlice, destinationLevel, destinationOrigin);
	}
	else
	{
		//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, TargetEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(Buffer, sourceOffset, sourceBytesPerRow, sourceBytesPerImage, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin, options));
        TargetEncoder->copyFromBuffer(Buffer->GetMTLBuffer().get(), sourceOffset + Buffer->GetOffset(), sourceBytesPerRow, sourceBytesPerImage, sourceSize,
                                      toTexture, destinationSlice, destinationLevel, destinationOrigin, options);
	}
	
	return false;
}

bool FMetalRenderPass::AsyncCopyFromTextureToTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin)
{
	MTL::BlitCommandEncoder* TargetEncoder;
	{
		ConditionalSwitchToBlit();
		TargetEncoder = CurrentEncoder.GetBlitCommandEncoder();
		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	}
	
	check(TargetEncoder);
	
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, TargetEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin));
    
    TargetEncoder->copyFromTexture(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize, toTexture, destinationSlice, destinationLevel, destinationOrigin);

	return false;
}

bool FMetalRenderPass::CanAsyncCopyToBuffer(FMetalBufferPtr DestinationBuffer)
{
	return false;
}

void FMetalRenderPass::AsyncCopyFromBufferToBuffer(FMetalBufferPtr SourceBuffer, NS::UInteger SourceOffset, FMetalBufferPtr DestinationBuffer, NS::UInteger DestinationOffset, NS::UInteger Size)
{
	MTL::BlitCommandEncoder* TargetEncoder;
	{
		ConditionalSwitchToBlit();
		TargetEncoder = CurrentEncoder.GetBlitCommandEncoder();
		METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), FString::Printf(TEXT("AsyncCopyFromBufferToBuffer: %p %llu %llu"), DestinationBuffer.Get(), DestinationOffset, Size)));
	}
	
	check(TargetEncoder);
	
    //MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, TargetEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Copy(SourceBuffer, SourceOffset, DestinationBuffer, DestinationOffset, Size));
    
    TargetEncoder->copyFromBuffer(SourceBuffer->GetMTLBuffer().get(), SourceOffset + SourceBuffer->GetOffset(),
                                  DestinationBuffer->GetMTLBuffer().get(), DestinationOffset + DestinationBuffer->GetOffset(), Size);
}

FMetalBufferPtr FMetalRenderPass::AllocateTemporyBufferForCopy(FMetalBufferPtr DestinationBuffer, NS::UInteger Size, NS::UInteger Align)
{
	FMetalBufferPtr Buffer;
    Buffer = CurrentEncoder.GetRingBuffer().NewBuffer(Size, Align);

	return Buffer;
}

void FMetalRenderPass::AsyncGenerateMipmapsForTexture(MTL::Texture* Texture)
{
	// This must be a plain old error
	ConditionalSwitchToBlit();
	MTL::BlitCommandEncoder* Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GenerateMipmaps(Texture));
    Encoder->generateMipmaps(Texture);
}

TRefCountPtr<FMetalFence> const& FMetalRenderPass::End(void)
{
	if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
	{
		State.FlushVisibilityResults(CurrentEncoder);
		check(!CurrentEncoderFence);
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	
	State.SetRenderTargetsActive(false);
	
	RenderPassDesc = nullptr;
	bWithinRenderPass = false;
	
	return CurrentEncoderFence;
}

void FMetalRenderPass::InsertCommandBufferFence(TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe>& Fence, FMetalCommandBufferCompletionHandler Handler)
{
	CurrentEncoder.InsertCommandBufferFence(Fence, Handler);
}

void FMetalRenderPass::AddCompletionHandler(FMetalCommandBufferCompletionHandler Handler)
{
	CurrentEncoder.AddCompletionHandler(Handler);
}

void FMetalRenderPass::AddAsyncCommandBufferHandlers(MTL::HandlerFunction Scheduled, MTL::HandlerFunction Completion)
{
	checkf(false, TEXT("Async command buffer has not been supported yet"));
}

void FMetalRenderPass::TransitionResources(MTL::Resource* Resource)
{
	CurrentEncoder.TransitionResources(Resource);
}

#pragma mark - Public Debug Support -

void FMetalRenderPass::InsertDebugEncoder()
{
	FMetalBufferPtr NewBuf = CurrentEncoder.GetRingBuffer().NewBuffer(BufferOffsetAlignment, BufferOffsetAlignment);
	
	check(NewBuf);
	
    MTL::BlitCommandEncoder* TargetEncoder = nullptr;
	ConditionalSwitchToBlit();
	TargetEncoder = CurrentEncoder.GetBlitCommandEncoder();
	METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CurrentEncoder.GetCommandBufferStats(), __FUNCTION__));
	
	check(TargetEncoder);
	
	//MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, TargetEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Fill(NewBuf, ns::Range(0, BufferOffsetAlignment), 0xff));
    
    TargetEncoder->fillBuffer(NewBuf->GetMTLBuffer().get(), NS::Range(NewBuf->GetOffset(), BufferOffsetAlignment), 0xff);
	
	ConditionalSubmit();
}

void FMetalRenderPass::InsertDebugSignpost(NS::String* String)
{
	CurrentEncoder.InsertDebugSignpost(String);
}

void FMetalRenderPass::PushDebugGroup(NS::String* String)
{
	CurrentEncoder.PushDebugGroup(String);
}

void FMetalRenderPass::PopDebugGroup(void)
{
	CurrentEncoder.PopDebugGroup();
}

#pragma mark - Public Accessors -
	
FMetalCommandBuffer* FMetalRenderPass::GetCurrentCommandBuffer(void)
{
	return CurrentEncoder.GetCommandBuffer();
}
	
FMetalSubBufferRing& FMetalRenderPass::GetRingBuffer(void)
{
	return CurrentEncoder.GetRingBuffer();
}

void FMetalRenderPass::ShrinkRingBuffers(void)
{
	CurrentEncoder.GetRingBuffer().Shrink();
}

void FMetalRenderPass::ConditionalSwitchToRender(void)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalSwitchToRenderTime);
	
	check(bWithinRenderPass);
	check(RenderPassDesc);
	check(CurrentEncoder.GetCommandBuffer());
	
	if (CurrentEncoder.IsComputeCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive())
	{
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	
	if (!CurrentEncoder.IsRenderCommandEncoderActive())
	{
		RestartRenderPass(nullptr);
	}
	
	check(CurrentEncoder.IsRenderCommandEncoderActive());
}

void FMetalRenderPass::ConditionalSwitchToCompute(void)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalSwitchToComputeTime);
	
	check(CurrentEncoder.GetCommandBuffer());
	
	if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive())
	{
		if (CurrentEncoder.IsRenderCommandEncoderActive())
		{
			State.SetRenderStoreActions(CurrentEncoder, true);
			State.FlushVisibilityResults(CurrentEncoder);
		}
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
		State.SetRenderTargetsActive(false);
	}
	
	if (!CurrentEncoder.IsComputeCommandEncoderActive())
	{
		State.SetStateDirty();
		CurrentEncoder.BeginComputeCommandEncoding(ComputeDispatchType);
		if (CurrentEncoderFence)
		{
			CurrentEncoder.WaitForFence(CurrentEncoderFence);
			CurrentEncoderFence = nullptr;
		}
	}
	
	check(CurrentEncoder.IsComputeCommandEncoderActive());
}

void FMetalRenderPass::ConditionalSwitchToBlit(void)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalSwitchToBlitTime);
	
	check(CurrentEncoder.GetCommandBuffer());
	
	if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
	{
		if (CurrentEncoder.IsRenderCommandEncoderActive())
		{
			State.SetRenderStoreActions(CurrentEncoder, true);
			State.FlushVisibilityResults(CurrentEncoder);
		}
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
		State.SetRenderTargetsActive(false);
	}
	
	if (!CurrentEncoder.IsBlitCommandEncoderActive())
	{
		CurrentEncoder.BeginBlitCommandEncoding();
		if (CurrentEncoderFence)
		{
			CurrentEncoder.WaitForFence(CurrentEncoderFence);
			CurrentEncoderFence = nullptr;
		}
	}
	
	check(CurrentEncoder.IsBlitCommandEncoderActive());
}

void FMetalRenderPass::CommitRenderResourceTables(void)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalCommitRenderResourceTablesTime);
	
	State.CommitRenderResources(&CurrentEncoder);
	
	if(!IsMetalBindlessEnabled())
	{
		State.CommitResourceTable(EMetalShaderStages::Vertex, MTL::FunctionTypeVertex, CurrentEncoder);
		
		FMetalGraphicsPipelineState const* BoundShaderState = State.GetGraphicsPSO();
		
		if (BoundShaderState->VertexShader->SideTableBinding >= 0)
		{
			CurrentEncoder.SetShaderSideTable(MTL::FunctionTypeVertex, BoundShaderState->VertexShader->SideTableBinding);
			State.SetShaderBuffer(EMetalShaderStages::Vertex, nullptr, nullptr, 0, 0, BoundShaderState->VertexShader->SideTableBinding, MTL::ResourceUsage(0));
		}
		
		if (IsValidRef(BoundShaderState->PixelShader))
		{
			State.CommitResourceTable(EMetalShaderStages::Pixel, MTL::FunctionTypeFragment, CurrentEncoder);
			if (BoundShaderState->PixelShader->SideTableBinding >= 0)
			{
				CurrentEncoder.SetShaderSideTable(MTL::FunctionTypeFragment, BoundShaderState->PixelShader->SideTableBinding);
				State.SetShaderBuffer(EMetalShaderStages::Pixel, nullptr, nullptr, 0, 0, BoundShaderState->PixelShader->SideTableBinding, MTL::ResourceUsage(0));
			}
		}
	}
}

void FMetalRenderPass::CommitDispatchResourceTables(void)
{
	State.CommitComputeResources(&CurrentEncoder);

	if(!IsMetalBindlessEnabled())
	{
		State.CommitResourceTable(EMetalShaderStages::Compute, MTL::FunctionTypeKernel, CurrentEncoder);
		
		FMetalComputeShader const* ComputeShader = State.GetComputeShader();
		if (ComputeShader->SideTableBinding >= 0)
		{
			CurrentEncoder.SetShaderSideTable(MTL::FunctionTypeKernel, ComputeShader->SideTableBinding);
			State.SetShaderBuffer(EMetalShaderStages::Compute, nullptr, nullptr, 0, 0, ComputeShader->SideTableBinding, MTL::ResourceUsage(0));
		}
		
#if METAL_RHI_RAYTRACING
		// TODO: Crappy workaround for inline raytracing support.
		if (ComputeShader->RayTracingBindings.InstanceIndexBuffer != UINT32_MAX && InstanceBufferSRV.IsValid())
		{
			FMetalRHIBuffer* SourceBuffer = ResourceCast(InstanceBufferSRV->GetBuffer());
			check(SourceBuffer);
			FMetalBuffer CurBuffer = SourceBuffer->GetCurrentBufferOrNil();
			check(CurBuffer);
			
			CurrentEncoder.SetShaderBuffer(mtlpp::FunctionType::Kernel, CurBuffer, InstanceBufferSRV->Offset, CurBuffer.GetLength(), ComputeShader->RayTracingBindings.InstanceIndexBuffer, MTL::ResourceUsage::Read);
			State.SetShaderBuffer(EMetalShaderStages::Compute, CurBuffer, nullptr, InstanceBufferSRV->Offset, CurBuffer.GetLength(), ComputeShader->RayTracingBindings.InstanceIndexBuffer, MTL::ResourceUsage::Read);
		}
#endif //METAL_RHI_RAYTRACING
	}
}

void FMetalRenderPass::PrepareToRender(uint32 PrimitiveType)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPrepareToRenderTime);
	
	check(CurrentEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsRenderCommandEncoderActive());
	
	// Set raster state
	State.SetRenderState(CurrentEncoder);
	
	// Bind shader resources
	CommitRenderResourceTables();
    
    State.SetRenderPipelineState(CurrentEncoder);
}

void FMetalRenderPass::PrepareToDispatch(void)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPrepareToDispatchTime);
	
	check(CurrentEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsComputeCommandEncoderActive());
	
	// Bind shader resources
	CommitDispatchResourceTables();
    
    State.SetComputePipelineState(CurrentEncoder);
}

void FMetalRenderPass::ConditionalSubmit()
{
	NumOutstandingOps++;
	
	bool bCanForceSubmit = State.CanRestartRenderPass();

	FRHIRenderPassInfo CurrentRenderTargets = State.GetRenderPassInfo();
	
	// Force a command-encoder when GMetalRuntimeDebugLevel is enabled to help track down intermittent command-buffer failures.
	if (GMetalCommandBufferCommitThreshold > 0 && NumOutstandingOps >= GMetalCommandBufferCommitThreshold && CmdList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelConditionalSubmit)
	{
		bool bCanChangeRT = true;
		
		if (bWithinRenderPass)
		{
			const bool bIsMSAAActive = State.GetHasValidRenderTarget() && State.GetSampleCount() != 1;
			bCanChangeRT = !bIsMSAAActive;
			
			for (int32 RenderTargetIndex = 0; bCanChangeRT && RenderTargetIndex < CurrentRenderTargets.GetNumColorRenderTargets(); RenderTargetIndex++)
			{
				FRHIRenderPassInfo::FColorEntry& RenderTargetView = CurrentRenderTargets.ColorRenderTargets[RenderTargetIndex];
				
				if (GetStoreAction(RenderTargetView.Action) != ERenderTargetStoreAction::EMultisampleResolve)
				{
					RenderTargetView.Action = MakeRenderTargetActions(ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
				}
				else
				{
					bCanChangeRT = false;
				}
			}
			
			if (bCanChangeRT && CurrentRenderTargets.DepthStencilRenderTarget.DepthStencilTarget)
			{
				if (GetStoreAction(GetDepthActions(CurrentRenderTargets.DepthStencilRenderTarget.Action)) != ERenderTargetStoreAction::EMultisampleResolve && GetStoreAction(GetStencilActions(CurrentRenderTargets.DepthStencilRenderTarget.Action)) != ERenderTargetStoreAction::EMultisampleResolve)
				{
					ERenderTargetActions Actions = MakeRenderTargetActions(ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
					CurrentRenderTargets.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(Actions, Actions);
				}
				else
				{
					bCanChangeRT = false;
				}
			}
		}
		
		bCanForceSubmit = bCanChangeRT;
	}
	
	if (GMetalCommandBufferCommitThreshold > 0 && NumOutstandingOps > 0 && NumOutstandingOps >= GMetalCommandBufferCommitThreshold && bCanForceSubmit)
	{
		if (CurrentEncoder.GetCommandBuffer())
		{
			Submit(EMetalSubmitFlagsCreateCommandBuffer);
			NumOutstandingOps = 0;
		}
		
		// Force a command-encoder when GMetalRuntimeDebugLevel is enabled to help track down intermittent command-buffer failures.
		if (bWithinRenderPass && CmdList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelConditionalSubmit && State.GetHasValidRenderTarget())
		{
			bool bSet = false;
			State.InvalidateRenderTargets();
			if (IsFeatureLevelSupported( GMaxRHIShaderPlatform, ERHIFeatureLevel::SM5 ))
			{
				bSet = State.SetRenderPassInfo(CurrentRenderTargets, State.GetVisibilityResultsBuffer(), false);
			}
			else
			{
				bSet = State.SetRenderPassInfo(CurrentRenderTargets, NULL, false);
			}
			
			if (bSet)
			{
				RestartRenderPass(State.GetRenderPassDescriptor());
			}
		}
	}
}

uint32 FMetalRenderPass::GetEncoderIndex(void) const
{
	return CurrentEncoder.NumEncodedPasses();
}

uint32 FMetalRenderPass::GetCommandBufferIndex(void) const
{
	return CurrentEncoder.GetCommandBufferIndex();
}
