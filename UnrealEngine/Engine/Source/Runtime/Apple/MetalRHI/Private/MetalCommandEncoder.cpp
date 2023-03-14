// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalShaderTypes.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalCommandBufferFence.h"
#include "MetalCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalComputeCommandEncoder.h"
#include "MetalRenderCommandEncoder.h"
#include "MetalProfiler.h"
#include "MetalShaderResources.h"

const uint32 EncoderRingBufferSize = 1024 * 1024;

#if METAL_DEBUG_OPTIONS
extern int32 GMetalBufferScribble;
#endif

static TCHAR const* const GMetalCommandDataTypeName[] = {
	TEXT("DrawPrimitive"),
	TEXT("DrawPrimitiveIndexed"),
	TEXT("DrawPrimitivePatch"),
	TEXT("DrawPrimitiveIndirect"),
	TEXT("DrawPrimitiveIndexedIndirect"),
	TEXT("Dispatch"),
	TEXT("DispatchIndirect"),
};


FString FMetalCommandData::ToString() const
{
	FString Result;
	if ((uint32)CommandType < (uint32)FMetalCommandData::Type::Num)
	{
		Result = GMetalCommandDataTypeName[(uint32)CommandType];
		switch(CommandType)
		{
			case FMetalCommandData::Type::DrawPrimitive:
				Result += FString::Printf(TEXT(" BaseInstance: %u InstanceCount: %u VertexCount: %u VertexStart: %u"), Draw.BaseInstance, Draw.InstanceCount, Draw.VertexCount, Draw.VertexStart);
				break;
			case FMetalCommandData::Type::DrawPrimitiveIndexed:
				Result += FString::Printf(TEXT(" BaseInstance: %u BaseVertex: %u IndexCount: %u IndexStart: %u InstanceCount: %u"), DrawIndexed.BaseInstance, DrawIndexed.BaseVertex, DrawIndexed.IndexCount, DrawIndexed.IndexStart, DrawIndexed.InstanceCount);
				break;
			case FMetalCommandData::Type::DrawPrimitivePatch:
				Result += FString::Printf(TEXT(" BaseInstance: %u InstanceCount: %u PatchCount: %u PatchStart: %u"), DrawPatch.BaseInstance, DrawPatch.InstanceCount, DrawPatch.PatchCount, DrawPatch.PatchStart);
				break;
			case FMetalCommandData::Type::Dispatch:
				Result += FString::Printf(TEXT(" X: %u Y: %u Z: %u"), (uint32)Dispatch.threadgroupsPerGrid[0], (uint32)Dispatch.threadgroupsPerGrid[1], (uint32)Dispatch.threadgroupsPerGrid[2]);
				break;
			case FMetalCommandData::Type::DispatchIndirect:
				Result += FString::Printf(TEXT(" Buffer: %p Offset: %u"), (void*)DispatchIndirect.ArgumentBuffer, (uint32)DispatchIndirect.ArgumentOffset);
				break;
			case FMetalCommandData::Type::DrawPrimitiveIndirect:
			case FMetalCommandData::Type::DrawPrimitiveIndexedIndirect:
			case FMetalCommandData::Type::Num:
			default:
				break;
		}
	}
	return Result;
};

struct FMetalCommandContextDebug
{
	TArray<FMetalCommandDebug> Commands;
	TSet<TRefCountPtr<FMetalGraphicsPipelineState>> PSOs;
	TSet<TRefCountPtr<FMetalComputeShader>> ComputeShaders;
	FMetalBuffer DebugBuffer;
};

@interface FMetalCommandBufferDebug : FApplePlatformObject
{
	@public
	TArray<FMetalCommandContextDebug> Contexts;
	uint32 Index;
}
@end
@implementation FMetalCommandBufferDebug
APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalCommandBufferDebug)
- (instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		Index = ~0u;
	}
	return Self;
}
- (void)dealloc
{
	Contexts.Empty();
	[super dealloc];
}
@end

char const* FMetalCommandBufferMarkers::kTableAssociationKey = "FMetalCommandBufferMarkers::kTableAssociationKey";

FMetalCommandBufferMarkers::FMetalCommandBufferMarkers(void)
: ns::Object<FMetalCommandBufferDebug*, ns::CallingConvention::ObjectiveC>(nil)
{
	
}

FMetalCommandBufferMarkers::FMetalCommandBufferMarkers(mtlpp::CommandBuffer& CmdBuf)
: ns::Object<FMetalCommandBufferDebug*, ns::CallingConvention::ObjectiveC>([FMetalCommandBufferDebug new], ns::Ownership::Assign)
{
	CmdBuf.SetAssociatedObject<FMetalCommandBufferMarkers>(FMetalCommandBufferMarkers::kTableAssociationKey, *this);
	m_ptr->Contexts.SetNum(1);
}


FMetalCommandBufferMarkers::FMetalCommandBufferMarkers(FMetalCommandBufferDebug* CmdBuf)
: ns::Object<FMetalCommandBufferDebug*, ns::CallingConvention::ObjectiveC>(CmdBuf)
{
	
}

void FMetalCommandBufferMarkers::AllocateContexts(uint32 NumContexts)
{
	if (m_ptr && m_ptr->Contexts.Num() < NumContexts)
	{
		m_ptr->Contexts.SetNum(NumContexts);
	}
}

uint32 FMetalCommandBufferMarkers::AddCommand(uint32 CmdBufIndex, uint32 Encoder, uint32 ContextIndex, FMetalBuffer& DebugBuffer, FMetalGraphicsPipelineState* PSO, FMetalComputeShader* ComputeShader, FMetalCommandData& Data)
{
	uint32 Num = 0;
	if (m_ptr)
	{
		if (m_ptr->Index == ~0u)
		{
			m_ptr->Index = CmdBufIndex;
		}
		
		FMetalCommandContextDebug& Context = m_ptr->Contexts[ContextIndex];
		if (Context.DebugBuffer != DebugBuffer)
		{
			Context.DebugBuffer = DebugBuffer;
		}
		
		if (PSO)
			Context.PSOs.Add(PSO);
		if (ComputeShader)
			Context.ComputeShaders.Add(ComputeShader);
		
		Num = Context.Commands.Num();
		FMetalCommandDebug Command;
        Command.CmdBufIndex = CmdBufIndex;
		Command.Encoder = Encoder;
		Command.Index = Num;
		Command.PSO = PSO;
		Command.ComputeShader = ComputeShader;
		Command.Data = Data;
		Context.Commands.Add(Command);
	}
	return Num;
}

TArray<FMetalCommandDebug>* FMetalCommandBufferMarkers::GetCommands(uint32 ContextIndex)
{
	TArray<FMetalCommandDebug>* Result = nullptr;
	if (m_ptr)
	{
		FMetalCommandContextDebug& Context = m_ptr->Contexts[ContextIndex];
		Result = &Context.Commands;
	}
	return Result;
}

ns::AutoReleased<FMetalBuffer> FMetalCommandBufferMarkers::GetDebugBuffer(uint32 ContextIndex)
{
	ns::AutoReleased<FMetalBuffer> Buffer;
	if (m_ptr)
	{
		FMetalCommandContextDebug& Context = m_ptr->Contexts[ContextIndex];
		Buffer = Context.DebugBuffer;
	}
	return Buffer;
}

uint32 FMetalCommandBufferMarkers::NumContexts() const
{
	uint32 Num = 0;
	if (m_ptr)
	{
		Num = m_ptr->Contexts.Num();
	}
	return Num;
}

uint32 FMetalCommandBufferMarkers::GetIndex() const
{
	uint32 Num = 0;
	if (m_ptr)
	{
		Num = m_ptr->Index;
	}
	return Num;
}

FMetalCommandBufferMarkers FMetalCommandBufferMarkers::Get(mtlpp::CommandBuffer const& CmdBuf)
{
	return CmdBuf.GetAssociatedObject<FMetalCommandBufferMarkers>(FMetalCommandBufferMarkers::kTableAssociationKey);
}

#pragma mark - Public C++ Boilerplate -

FMetalCommandEncoder::FMetalCommandEncoder(FMetalCommandList& CmdList, EMetalCommandEncoderType InType)
: CommandList(CmdList)
, bSupportsMetalFeaturesSetBytes(CmdList.GetCommandQueue().SupportsFeature(EMetalFeaturesSetBytes))
, RingBuffer(EncoderRingBufferSize, BufferOffsetAlignment, FMetalCommandQueue::GetCompatibleResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::HazardTrackingModeUntracked | BUFFER_RESOURCE_STORAGE_MANAGED)))
, RenderPassDesc(nil)
, EncoderFence(nil)
#if ENABLE_METAL_GPUPROFILE
, CommandBufferStats(nullptr)
#endif
#if METAL_DEBUG_OPTIONS
, WaitCount(0)
, UpdateCount(0)
#endif
, DebugGroups([NSMutableArray new])
, FenceStage(mtlpp::RenderStages::Fragment)
, EncoderNum(0)
, CmdBufIndex(0)
, Type(InType)
{
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		FMemory::Memzero(ShaderBuffers[Frequency].Usage);
		ShaderBuffers[Frequency].SideTable = [[FMetalBufferData alloc] init];
		ShaderBuffers[Frequency].SideTable->Data = (uint8*)(&ShaderBuffers[Frequency].Lengths[0]);
		ShaderBuffers[Frequency].SideTable->Len = sizeof(ShaderBuffers[Frequency].Lengths);
		ShaderBuffers[Frequency].Bound = 0;
	}
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStoreActions[i] = mtlpp::StoreAction::Unknown;
	}
	DepthStoreAction = mtlpp::StoreAction::Unknown;
	StencilStoreAction = mtlpp::StoreAction::Unknown;
}

FMetalCommandEncoder::~FMetalCommandEncoder(void)
{
	if(CommandBuffer)
	{
		EndEncoding();
		CommitCommandBuffer(false);
	}
	
	check(!IsRenderCommandEncoderActive());
	check(!IsComputeCommandEncoderActive());
	check(!IsBlitCommandEncoderActive());
	
	SafeReleaseMetalRenderPassDescriptor(RenderPassDesc);
	RenderPassDesc = nil;

	if(DebugGroups)
	{
		[DebugGroups release];
	}
	
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		FMemory::Memzero(ShaderBuffers[Frequency].Usage);
		ShaderBuffers[Frequency].SideTable->Data = nullptr;
		[ShaderBuffers[Frequency].SideTable release];
		ShaderBuffers[Frequency].SideTable = nil;
		ShaderBuffers[Frequency].Bound = 0;
	}
}

void FMetalCommandEncoder::Reset(void)
{
    check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);
	
	if(RenderPassDesc)
	{
		SafeReleaseMetalRenderPassDescriptor(RenderPassDesc);
		RenderPassDesc = nil;
	}
	
	{
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
		{
			ColorStoreActions[i] = mtlpp::StoreAction::Unknown;
		}
		DepthStoreAction = mtlpp::StoreAction::Unknown;
		StencilStoreAction = mtlpp::StoreAction::Unknown;
	}
	
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
    	FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		FMemory::Memzero(ShaderBuffers[Frequency].Usage);
		ShaderBuffers[Frequency].Bound = 0;
	}
	
	[DebugGroups removeAllObjects];
}

void FMetalCommandEncoder::ResetLive(void)
{
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		ShaderBuffers[Frequency].Bound = 0;
	}
	
	if (IsRenderCommandEncoderActive())
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			RenderCommandEncoder.SetVertexBuffer(nil, 0, i);
			RenderCommandEncoder.SetFragmentBuffer(nil, 0, i);
		}
		
		for (uint32 i = 0; i < ML_MaxTextures; i++)
		{
			RenderCommandEncoder.SetVertexTexture(nil, i);
			RenderCommandEncoder.SetFragmentTexture(nil, i);
		}
	}
	else if (IsComputeCommandEncoderActive())
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ComputeCommandEncoder.SetBuffer(nil, 0, i);
		}
		
		for (uint32 i = 0; i < ML_MaxTextures; i++)
		{
			ComputeCommandEncoder.SetTexture(nil, i);
		}
	}
}

#pragma mark - Public Command Buffer Mutators -

void FMetalCommandEncoder::StartCommandBuffer(void)
{
	check(!CommandBuffer || EncoderNum == 0);
	check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);

	if (!CommandBuffer)
	{
		CmdBufIndex++;
		CommandBuffer = CommandList.GetCommandQueue().CreateCommandBuffer();
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, CommandBufferDebug = FMetalCommandBufferDebugging::Get(CommandBuffer));
		
		if (GMetalCommandBufferDebuggingEnabled)
		{
			CommandBufferMarkers = FMetalCommandBufferMarkers(CommandBuffer);
		}
		
		if ([DebugGroups count] > 0)
		{
			CommandBuffer.SetLabel([DebugGroups lastObject]);
		}
		
	#if ENABLE_METAL_GPUPROFILE
		FMetalProfiler* Profiler = FMetalProfiler::GetProfiler();
		if (Profiler)
		{
			CommandBufferStats = Profiler->AllocateCommandBuffer(CommandBuffer, 0);
		}
	#endif
	}
}
	
void FMetalCommandEncoder::CommitCommandBuffer(uint32 const Flags)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);

	bool const bWait = (Flags & EMetalSubmitFlagsWaitOnCommandBuffer);
	bool const bIsLastCommandBuffer = (Flags & EMetalSubmitFlagsLastCommandBuffer);
	
	if (EncoderNum == 0 && !bWait && !(Flags & EMetalSubmitFlagsForce))
	{
		return;
	}
	
	if(CommandBuffer.GetLabel() == nil && [DebugGroups count] > 0)
	{
		CommandBuffer.SetLabel([DebugGroups lastObject]);
	}
	
	if (!(Flags & EMetalSubmitFlagsBreakCommandBuffer))
	{
		RingBuffer.Commit(CommandBuffer);
	}
	else
	{
		RingBuffer.Submit();
	}
    
#if METAL_DEBUG_OPTIONS
    if(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
    {
        for (FMetalBuffer const& Buffer : ActiveBuffers)
        {
            GetMetalDeviceContext().AddActiveBuffer(Buffer);
        }
        
        TSet<ns::AutoReleased<FMetalBuffer>> NewActiveBuffers = MoveTemp(ActiveBuffers);
        AddCompletionHandler([NewActiveBuffers](mtlpp::CommandBuffer const&)
                             {
                                 for (FMetalBuffer const& Buffer : NewActiveBuffers)
                                 {
                                     GetMetalDeviceContext().RemoveActiveBuffer(Buffer);
                                 }
                             });
    }
#endif
#if ENABLE_METAL_GPUPROFILE
	CommandBufferStats->End(CommandBuffer);
	CommandBufferStats = nullptr;
#endif

	CommandList.Commit(CommandBuffer, MoveTemp(CompletionHandlers), bWait, bIsLastCommandBuffer);
	
	CommandBuffer = nil;
	if (Flags & EMetalSubmitFlagsCreateCommandBuffer)
	{
		StartCommandBuffer();
		check(CommandBuffer);
	}
	
	BufferBindingHistory.Empty();
	TextureBindingHistory.Empty();
	
	EncoderNum = 0;
}

#pragma mark - Public Command Encoder Accessors -
	
bool FMetalCommandEncoder::IsParallelRenderCommandEncoderActive(void) const
{
	return ParallelRenderCommandEncoder.GetPtr() != nil;
}
	
bool FMetalCommandEncoder::IsRenderCommandEncoderActive(void) const
{
	return RenderCommandEncoder.GetPtr() != nil || ParallelRenderCommandEncoder.GetPtr() != nil;
}

bool FMetalCommandEncoder::IsComputeCommandEncoderActive(void) const
{
	return ComputeCommandEncoder.GetPtr() != nil;
}

bool FMetalCommandEncoder::IsBlitCommandEncoderActive(void) const
{
	return BlitCommandEncoder.GetPtr() != nil;
}

bool FMetalCommandEncoder::IsImmediate(void) const
{
	return CommandList.IsImmediate();
}

bool FMetalCommandEncoder::IsParallel(void) const
{
	return CommandList.IsParallel() && (Type == EMetalCommandEncoderCurrent);
}

bool FMetalCommandEncoder::IsRenderPassDescriptorValid(void) const
{
	return (RenderPassDesc != nil);
}

mtlpp::RenderPassDescriptor const& FMetalCommandEncoder::GetRenderPassDescriptor(void) const
{
	return RenderPassDesc;
}

mtlpp::ParallelRenderCommandEncoder& FMetalCommandEncoder::GetParallelRenderCommandEncoder(void)
{
	return ParallelRenderCommandEncoder;
}

mtlpp::RenderCommandEncoder& FMetalCommandEncoder::GetChildRenderCommandEncoder(uint32 Index)
{
	check(IsParallelRenderCommandEncoderActive() && Index < ChildRenderCommandEncoders.Num());
	return ChildRenderCommandEncoders[Index];
}

mtlpp::RenderCommandEncoder& FMetalCommandEncoder::GetRenderCommandEncoder(void)
{
	check(IsRenderCommandEncoderActive() && RenderCommandEncoder);
	return RenderCommandEncoder;
}

mtlpp::ComputeCommandEncoder& FMetalCommandEncoder::GetComputeCommandEncoder(void)
{
	check(IsComputeCommandEncoderActive());
	return ComputeCommandEncoder;
}

mtlpp::BlitCommandEncoder& FMetalCommandEncoder::GetBlitCommandEncoder(void)
{
	check(IsBlitCommandEncoderActive());
	return BlitCommandEncoder;
}

TRefCountPtr<FMetalFence> const& FMetalCommandEncoder::GetEncoderFence(void) const
{
	return EncoderFence;
}
	
#pragma mark - Public Command Encoder Mutators -

void FMetalCommandEncoder::BeginParallelRenderCommandEncoding(uint32 NumChildren)
{
	check(IsImmediate());
	check(RenderPassDesc);
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	FenceResources.Append(TransitionedResources);
	
	ParallelRenderCommandEncoder = MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ParallelRenderCommandEncoder(RenderPassDesc));
	METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ParallelEncoderDebug = FMetalParallelRenderCommandEncoderDebugging(ParallelRenderCommandEncoder, RenderPassDesc, CommandBufferDebug));
	
	EncoderNum++;
	
	check(!EncoderFence);
	NSString* Label = nil;
	
	if(GetEmitDrawEvents())
	{
		Label = [NSString stringWithFormat:@"ParallelRenderCommandEncoder: %@", [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass")];
		ParallelRenderCommandEncoder.SetLabel(Label);
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				ParallelRenderCommandEncoder.PushDebugGroup(Group);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.PushDebugGroup(Group));
			}
		}
	}
	
	for (uint32 i = 0; i < NumChildren; i++)
	{
		mtlpp::RenderCommandEncoder CommandEncoder = MTLPP_VALIDATE(mtlpp::ParallelRenderCommandEncoder, ParallelRenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetRenderCommandEncoder());
		ChildRenderCommandEncoders.Add(CommandEncoder);
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ParallelEncoderDebug.GetRenderCommandEncoderDebugger(CommandEncoder));
	}
}

void FMetalCommandEncoder::BeginRenderCommandEncoding(void)
{
	check(RenderPassDesc);
	check(CommandList.IsParallel() || CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	FenceResources.Append(TransitionedResources);
	
	if (!CommandList.IsParallel() || Type == EMetalCommandEncoderPrologue)
	{
		RenderCommandEncoder = MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, RenderCommandEncoder(RenderPassDesc));
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug = FMetalRenderCommandEncoderDebugging(RenderCommandEncoder, RenderPassDesc, CommandBufferDebug));
		EncoderNum++;	
	}
	else
	{
		RenderCommandEncoder = GetMetalDeviceContext().GetParallelRenderCommandEncoder(CommandList.GetParallelIndex(), ParallelRenderCommandEncoder, CommandBuffer);
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug = FMetalRenderCommandEncoderDebugging::Get(RenderCommandEncoder));
	}
	
	check(!EncoderFence);
	NSString* Label = nil;
	
	if(GetEmitDrawEvents())
	{
		Label = [NSString stringWithFormat:@"RenderEncoder: %@", [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass")];
		RenderCommandEncoder.SetLabel(Label);
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				RenderCommandEncoder.PushDebugGroup(Group);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.PushDebugGroup(Group));
			}
		}
	}
	
	if (CommandList.IsImmediate())
	{
		EncoderFence = CommandList.GetCommandQueue().CreateFence(Label);
	}
}

void FMetalCommandEncoder::BeginComputeCommandEncoding(mtlpp::DispatchType DispatchType)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	FenceResources.Append(TransitionedResources);
	TransitionedResources.Empty();
	
	if (DispatchType == mtlpp::DispatchType::Serial)
	{
		ComputeCommandEncoder = MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ComputeCommandEncoder());
	}
	else
	{
		ComputeCommandEncoder = MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ComputeCommandEncoder(DispatchType));
	}
	METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug = FMetalComputeCommandEncoderDebugging(ComputeCommandEncoder, CommandBufferDebug));

	EncoderNum++;
	
	check(!EncoderFence);
	NSString* Label = nil;
	
	if(GetEmitDrawEvents())
	{
		Label = [NSString stringWithFormat:@"ComputeEncoder: %@", [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass")];
		ComputeCommandEncoder.SetLabel(Label);
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				ComputeCommandEncoder.PushDebugGroup(Group);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.PushDebugGroup(Group));
			}
		}
	}
	
	EncoderFence = CommandList.GetCommandQueue().CreateFence(Label);
}

void FMetalCommandEncoder::BeginBlitCommandEncoding(void)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	FenceResources.Append(TransitionedResources);
	TransitionedResources.Empty();
	
	BlitCommandEncoder = MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, BlitCommandEncoder());
	METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug = FMetalBlitCommandEncoderDebugging(BlitCommandEncoder, CommandBufferDebug));
	
	EncoderNum++;
	
	check(!EncoderFence);
	NSString* Label = nil;
	
	if(GetEmitDrawEvents())
	{
		Label = [NSString stringWithFormat:@"BlitEncoder: %@", [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass")];
		BlitCommandEncoder.SetLabel(Label);
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				BlitCommandEncoder.PushDebugGroup(Group);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.PushDebugGroup(Group));
			}
		}
	}
	
	EncoderFence = CommandList.GetCommandQueue().CreateFence(Label);
}

TRefCountPtr<FMetalFence> FMetalCommandEncoder::EndEncoding(void)
{
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	TRefCountPtr<FMetalFence> Fence = nullptr;
	@autoreleasepool
	{
		if(IsRenderCommandEncoderActive())
		{
			if (RenderCommandEncoder)
			{
				check(!bSupportsFences || EncoderFence || !CommandList.IsImmediate());
				if (ParallelRenderCommandEncoder.GetPtr() == nil)
				{
					check(RenderPassDesc);
					
					ns::Array<mtlpp::RenderPassColorAttachmentDescriptor> ColorAttachments = RenderPassDesc.GetColorAttachments();
					for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
					{
						if (ColorAttachments[i].GetTexture() && ColorAttachments[i].GetStoreAction() == mtlpp::StoreAction::Unknown)
						{
							mtlpp::StoreAction Action = ColorStoreActions[i];
							check(Action != mtlpp::StoreAction::Unknown);
							RenderCommandEncoder.SetColorStoreAction((mtlpp::StoreAction)Action, i);
						}
					}
					if (RenderPassDesc.GetDepthAttachment().GetTexture() && RenderPassDesc.GetDepthAttachment().GetStoreAction() == mtlpp::StoreAction::Unknown)
					{
						mtlpp::StoreAction Action = DepthStoreAction;
						check(Action != mtlpp::StoreAction::Unknown);
						RenderCommandEncoder.SetDepthStoreAction((mtlpp::StoreAction)Action);
					}
					if (RenderPassDesc.GetStencilAttachment().GetTexture() && RenderPassDesc.GetStencilAttachment().GetStoreAction() == mtlpp::StoreAction::Unknown)
					{
						mtlpp::StoreAction Action = StencilStoreAction;
						check(Action != mtlpp::StoreAction::Unknown);
						RenderCommandEncoder.SetStencilStoreAction((mtlpp::StoreAction)Action);
					}
				}
				
				for (FMetalFence* FragFence : FragmentFences)
				{
					if (FragFence->NeedsWait(mtlpp::RenderStages::Fragment))
					{
						mtlpp::Fence FragmentFence = FragFence->Get(mtlpp::RenderStages::Fragment);
						mtlpp::Fence FragInnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)FragmentFence.GetPtr()).Inner) :) FragmentFence;
						
						RenderCommandEncoder.WaitForFence(FragInnerFence, FenceStage);
						METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddWaitFence(FragmentFence));
						FragFence->Wait(mtlpp::RenderStages::Fragment);
					}
				}
				FragmentFences.Empty();
				
				if (FenceStage == mtlpp::RenderStages::Vertex)
				{
					FenceResources.Empty();
					FenceStage = mtlpp::RenderStages::Fragment;
				}
				
				if (EncoderFence && EncoderFence->NeedsWrite(mtlpp::RenderStages::Fragment))
				{
					Fence = EncoderFence;
				}
				UpdateFence(EncoderFence);
				
#if METAL_DEBUG_OPTIONS
				if (bSupportsFences && SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation && (!WaitCount || !UpdateCount))
				{
					UE_LOG(LogMetal, Error, TEXT("%s has incorrect fence waits (%u) vs. updates (%u)."), *FString(RenderCommandEncoder.GetLabel()), WaitCount, UpdateCount);
					
				}
				WaitCount = 0;
				UpdateCount = 0;
#endif
				RenderCommandEncoder.EndEncoding();
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.EndEncoder());
				RenderCommandEncoder = nil;
				EncoderFence = nullptr;
			}
			
			if (ParallelRenderCommandEncoder && IsParallel())
			{
				RingBuffer.Commit(CommandBuffer);
				
			#if METAL_DEBUG_OPTIONS
				if(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
				{
					for (FMetalBuffer const& Buffer : ActiveBuffers)
					{
						GetMetalDeviceContext().AddActiveBuffer(Buffer);
					}
					
					TSet<ns::AutoReleased<FMetalBuffer>> NewActiveBuffers = MoveTemp(ActiveBuffers);
					AddCompletionHandler([NewActiveBuffers](mtlpp::CommandBuffer const&)
										{
											for (FMetalBuffer const& Buffer : NewActiveBuffers)
											{
												GetMetalDeviceContext().RemoveActiveBuffer(Buffer);
											}
										});
				}
			#endif

				BufferBindingHistory.Empty();
				TextureBindingHistory.Empty();
				
				EncoderNum = 0;

				CommandBuffer = nil;
				
				ParallelRenderCommandEncoder = nil;
			}

			if (ParallelRenderCommandEncoder && IsImmediate())
			{
				{
					check(RenderPassDesc);
					
					ns::Array<mtlpp::RenderPassColorAttachmentDescriptor> ColorAttachments = RenderPassDesc.GetColorAttachments();
					for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
					{
						if (ColorAttachments[i].GetTexture() && ColorAttachments[i].GetStoreAction() == mtlpp::StoreAction::Unknown)
						{
							mtlpp::StoreAction Action = ColorStoreActions[i];
							check(Action != mtlpp::StoreAction::Unknown);
							ParallelRenderCommandEncoder.SetColorStoreAction((mtlpp::StoreAction)Action, i);
						}
					}
					if (RenderPassDesc.GetDepthAttachment().GetTexture() && RenderPassDesc.GetDepthAttachment().GetStoreAction() == mtlpp::StoreAction::Unknown)
					{
						mtlpp::StoreAction Action = DepthStoreAction;
						check(Action != mtlpp::StoreAction::Unknown);
						ParallelRenderCommandEncoder.SetDepthStoreAction((mtlpp::StoreAction)Action);
					}
					if (RenderPassDesc.GetStencilAttachment().GetTexture() && RenderPassDesc.GetStencilAttachment().GetStoreAction() == mtlpp::StoreAction::Unknown)
					{
						mtlpp::StoreAction Action = StencilStoreAction;
						check(Action != mtlpp::StoreAction::Unknown);
						ParallelRenderCommandEncoder.SetStencilStoreAction((mtlpp::StoreAction)Action);
					}
				}

				ParallelRenderCommandEncoder.EndEncoding();
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ParallelEncoderDebug.EndEncoder());
				ParallelRenderCommandEncoder = nil;

				ChildRenderCommandEncoders.Empty();
			}
		}
		else if(IsComputeCommandEncoderActive())
		{
			check(!bSupportsFences || EncoderFence);
			
			for (FMetalFence* FragFence : FragmentFences)
			{
				if (FragFence->NeedsWait(mtlpp::RenderStages::Fragment))
				{
					mtlpp::Fence FragmentFence = FragFence->Get(mtlpp::RenderStages::Fragment);
					mtlpp::Fence FragInnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)FragmentFence.GetPtr()).Inner) :) FragmentFence;
					
					ComputeCommandEncoder.WaitForFence(FragInnerFence);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddWaitFence(FragmentFence));
					FragFence->Wait(mtlpp::RenderStages::Fragment);
				}
			}
			FragmentFences.Empty();
			FenceResources.Empty();
			FenceStage = mtlpp::RenderStages::Fragment;
			
			if (EncoderFence && EncoderFence->NeedsWrite(mtlpp::RenderStages::Fragment))
			{
				Fence = EncoderFence;
			}
			UpdateFence(EncoderFence);
			
#if METAL_DEBUG_OPTIONS
			if (bSupportsFences && SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation && (!WaitCount || !UpdateCount))
			{
				UE_LOG(LogMetal, Error, TEXT("%s has incorrect fence waits (%u) vs. updates (%u)."), *FString(ComputeCommandEncoder.GetLabel()), WaitCount, UpdateCount);
				
			}
			WaitCount = 0;
			UpdateCount = 0;
#endif
			ComputeCommandEncoder.EndEncoding();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.EndEncoder());
			ComputeCommandEncoder = nil;
			EncoderFence = nullptr;
		}
		else if(IsBlitCommandEncoderActive())
		{
			// check(!bSupportsFences || EncoderFence);
			
			for (FMetalFence* FragFence : FragmentFences)
			{
				if (FragFence->NeedsWait(mtlpp::RenderStages::Fragment))
				{
					mtlpp::Fence FragmentFence = FragFence->Get(mtlpp::RenderStages::Fragment);
					mtlpp::Fence FragInnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)FragmentFence.GetPtr()).Inner) :) FragmentFence;
					
					BlitCommandEncoder.WaitForFence(FragInnerFence);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddWaitFence(FragmentFence));
					FragFence->Wait(mtlpp::RenderStages::Fragment);
				}
			}
			FragmentFences.Empty();
			FenceResources.Empty();
			FenceStage = mtlpp::RenderStages::Fragment;
			
			if (EncoderFence && EncoderFence->NeedsWrite(mtlpp::RenderStages::Fragment))
			{
				Fence = EncoderFence;
			}
			UpdateFence(EncoderFence);
			
#if METAL_DEBUG_OPTIONS
			if (bSupportsFences && SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation && (!WaitCount || !UpdateCount))
			{
				UE_LOG(LogMetal, Error, TEXT("%s has incorrect fence waits (%u) vs. updates (%u)."), *FString(BlitCommandEncoder.GetLabel()), WaitCount, UpdateCount);
				
			}
			WaitCount = 0;
			UpdateCount = 0;
#endif
			BlitCommandEncoder.EndEncoding();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.EndEncoder());
			BlitCommandEncoder = nil;
			EncoderFence = nullptr;
		}
	}
	
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		FMemory::Memzero(ShaderBuffers[Frequency].Usage);
		ShaderBuffers[Frequency].Bound = 0;
	}
    return Fence;
}

void FMetalCommandEncoder::InsertCommandBufferFence(FMetalCommandBufferFence& Fence, mtlpp::CommandBufferHandler Handler)
{
	check(CommandBuffer);
	
	Fence.CommandBufferFence = CommandBuffer.GetCompletionFence();
	
	if (Handler)
	{
		AddCompletionHandler(Handler);
	}
}

void FMetalCommandEncoder::AddCompletionHandler(mtlpp::CommandBufferHandler Handler)
{
	check(Handler);
	
	mtlpp::CommandBufferHandler HeapHandler = Block_copy(Handler);
	CompletionHandlers.Add(HeapHandler);
	Block_release(HeapHandler);
}

void FMetalCommandEncoder::UpdateFence(FMetalFence* Fence)
{
	check(IsRenderCommandEncoderActive() || IsComputeCommandEncoderActive() || IsBlitCommandEncoderActive());
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	if ((bSupportsFences METAL_DEBUG_OPTION(|| CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)) && Fence)
	{
		mtlpp::Fence VertexFence = Fence->Get(mtlpp::RenderStages::Vertex);
		mtlpp::Fence InnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)VertexFence.GetPtr()).Inner) :) VertexFence;
		if (RenderCommandEncoder)
		{
			mtlpp::Fence FragmentFence = Fence->Get(mtlpp::RenderStages::Fragment);
			mtlpp::Fence FragInnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)FragmentFence.GetPtr()).Inner) :) FragmentFence;
			
			if (Fence->NeedsWrite(mtlpp::RenderStages::Vertex))
			{
				RenderCommandEncoder.UpdateFence(InnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddUpdateFence(VertexFence));
				Fence->Write(mtlpp::RenderStages::Vertex);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, UpdateCount++);
			}

			if (Fence->NeedsWrite(mtlpp::RenderStages::Fragment))
			{
				RenderCommandEncoder.UpdateFence(FragInnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Fragment));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddUpdateFence(FragmentFence));
				Fence->Write(mtlpp::RenderStages::Fragment);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, UpdateCount++);
			}
		}
		else if (ComputeCommandEncoder && Fence->NeedsWrite(mtlpp::RenderStages::Vertex))
		{
			ComputeCommandEncoder.UpdateFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddUpdateFence(VertexFence));
			Fence->Write(mtlpp::RenderStages::Vertex);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, UpdateCount++);
		}
		else if (BlitCommandEncoder && Fence->NeedsWrite(mtlpp::RenderStages::Vertex))
		{
			BlitCommandEncoder.UpdateFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddUpdateFence(VertexFence));
			Fence->Write(mtlpp::RenderStages::Vertex);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, UpdateCount++);
		}
	}
}

void FMetalCommandEncoder::WaitForFence(FMetalFence* Fence)
{
	check(IsRenderCommandEncoderActive() || IsComputeCommandEncoderActive() || IsBlitCommandEncoderActive());
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	if ((bSupportsFences METAL_DEBUG_OPTION(|| CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)) && Fence)
	{
		if (Fence->NeedsWait(mtlpp::RenderStages::Vertex))
		{
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, WaitCount++);
			
			mtlpp::Fence VertexFence = Fence->Get(mtlpp::RenderStages::Vertex);
			mtlpp::Fence InnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)VertexFence.GetPtr()).Inner) :) VertexFence;
			if (RenderCommandEncoder)
			{
				RenderCommandEncoder.WaitForFence(InnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddWaitFence(VertexFence));
				Fence->Wait(mtlpp::RenderStages::Vertex);
			}
			else if (ComputeCommandEncoder)
			{
				ComputeCommandEncoder.WaitForFence(InnerFence);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddWaitFence(VertexFence));
				Fence->Wait(mtlpp::RenderStages::Vertex);
			}
			else if (BlitCommandEncoder)
			{
				BlitCommandEncoder.WaitForFence(InnerFence);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddWaitFence(VertexFence));
				Fence->Wait(mtlpp::RenderStages::Vertex);
			}
		}
		if (Fence->NeedsWait(mtlpp::RenderStages::Fragment))
		{
			if (FenceStage == mtlpp::RenderStages::Vertex || BlitCommandEncoder)
			{
				mtlpp::Fence FragmentFence = Fence->Get(mtlpp::RenderStages::Fragment);
				mtlpp::Fence FragInnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)FragmentFence.GetPtr()).Inner) :) FragmentFence;
				if (RenderCommandEncoder)
				{
					RenderCommandEncoder.WaitForFence(FragInnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex));
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddWaitFence(FragmentFence));
					Fence->Wait(mtlpp::RenderStages::Fragment);
				}
				else if (ComputeCommandEncoder)
				{
					ComputeCommandEncoder.WaitForFence(FragInnerFence);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddWaitFence(FragmentFence));
					Fence->Wait(mtlpp::RenderStages::Fragment);
				}
				else if (BlitCommandEncoder)
				{
					BlitCommandEncoder.WaitForFence(FragInnerFence);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddWaitFence(FragmentFence));
					Fence->Wait(mtlpp::RenderStages::Fragment);
				}
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, WaitCount++);
			}
			else
			{
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, WaitCount++);
				FragmentFences.Add(Fence);
			}
		}
	}
}

void FMetalCommandEncoder::WaitAndUpdateFence(FMetalFence* Fence)
{
	check(IsRenderCommandEncoderActive() || IsComputeCommandEncoderActive() || IsBlitCommandEncoderActive());
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	if ((bSupportsFences METAL_DEBUG_OPTION(|| CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)) && Fence)
	{
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, WaitCount++);
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, UpdateCount++);
		
		mtlpp::Fence VertexFence = Fence->Get(mtlpp::RenderStages::Vertex);
		mtlpp::Fence InnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)VertexFence.GetPtr()).Inner) :) VertexFence;
		if (RenderCommandEncoder)
		{
			mtlpp::Fence FragmentFence = Fence->Get(mtlpp::RenderStages::Fragment);
			mtlpp::Fence FragInnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)FragmentFence.GetPtr()).Inner) :) FragmentFence;
			
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, WaitCount++);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, UpdateCount++);
			
			RenderCommandEncoder.WaitForFence(InnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddWaitFence(VertexFence));
			Fence->Wait(mtlpp::RenderStages::Vertex);
			
			RenderCommandEncoder.WaitForFence(FragInnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Fragment));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddWaitFence(FragmentFence));
			Fence->Wait(mtlpp::RenderStages::Fragment);
			
			RenderCommandEncoder.UpdateFence(InnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Fragment));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddUpdateFence(VertexFence));
			Fence->Write(mtlpp::RenderStages::Vertex);
			
			RenderCommandEncoder.UpdateFence(FragInnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddUpdateFence(FragmentFence));
			Fence->Write(mtlpp::RenderStages::Fragment);
		}
		else if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.WaitForFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddWaitFence(VertexFence));
			Fence->Wait(mtlpp::RenderStages::Vertex);
			
			ComputeCommandEncoder.UpdateFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddUpdateFence(VertexFence));
			Fence->Write(mtlpp::RenderStages::Vertex);
		}
		else if (BlitCommandEncoder)
		{
			BlitCommandEncoder.WaitForFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddWaitFence(VertexFence));
			Fence->Wait(mtlpp::RenderStages::Vertex);
			
			BlitCommandEncoder.UpdateFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddUpdateFence(VertexFence));
			Fence->Write(mtlpp::RenderStages::Vertex);
		}
	}
}

#pragma mark - Public Debug Support -

void FMetalCommandEncoder::InsertDebugSignpost(ns::String const& String)
{
	if (String)
	{
		if (RenderCommandEncoder)
		{
			RenderCommandEncoder.InsertDebugSignpost(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.InsertDebugSignpost(String));
		}
		else if (ParallelRenderCommandEncoder && !IsParallel())
		{
			ParallelRenderCommandEncoder.InsertDebugSignpost(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ParallelRenderCommandEncoder.InsertDebugSignpost(String));
		}
		else if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.InsertDebugSignpost(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.InsertDebugSignpost(String));
		}
		else if (BlitCommandEncoder)
		{
			BlitCommandEncoder.InsertDebugSignpost(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.InsertDebugSignpost(String));
		}
	}
}

void FMetalCommandEncoder::PushDebugGroup(ns::String const& String)
{
	if (String)
	{
		[DebugGroups addObject:String];
		if (RenderCommandEncoder)
		{
			RenderCommandEncoder.PushDebugGroup(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.PushDebugGroup(String));
		}
		else if (ParallelRenderCommandEncoder && !IsParallel())
		{
			ParallelRenderCommandEncoder.PushDebugGroup(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ParallelRenderCommandEncoder.PushDebugGroup(String));
		}
		else if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.PushDebugGroup(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.PushDebugGroup(String));
		}
		else if (BlitCommandEncoder)
		{
			BlitCommandEncoder.PushDebugGroup(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.PushDebugGroup(String));
		}
	}
}

void FMetalCommandEncoder::PopDebugGroup(void)
{
	if (DebugGroups.count > 0)
	{
		[DebugGroups removeLastObject];
		if (RenderCommandEncoder)
		{
			RenderCommandEncoder.PopDebugGroup();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.PopDebugGroup());
		}
		else if (ParallelRenderCommandEncoder && !IsParallel())
		{
			ParallelRenderCommandEncoder.PopDebugGroup();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ParallelRenderCommandEncoder.PopDebugGroup());
		}
		else if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.PopDebugGroup();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.PopDebugGroup());
		}
		else if (BlitCommandEncoder)
		{
			BlitCommandEncoder.PopDebugGroup();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.PopDebugGroup());
		}
	}
}

FMetalCommandBufferMarkers& FMetalCommandEncoder::GetMarkers(void)
{
	return CommandBufferMarkers;
}

#if ENABLE_METAL_GPUPROFILE
FMetalCommandBufferStats* FMetalCommandEncoder::GetCommandBufferStats(void)
{
	return CommandBufferStats;
}
#endif

#pragma mark - Public Render State Mutators -

void FMetalCommandEncoder::SetRenderPassDescriptor(mtlpp::RenderPassDescriptor RenderPass)
{
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	check(RenderPass);
	
	if(RenderPass.GetPtr() != RenderPassDesc.GetPtr())
	{
		SafeReleaseMetalRenderPassDescriptor(RenderPassDesc);
		RenderPassDesc = RenderPass;
		{
			for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
			{
				ColorStoreActions[i] = mtlpp::StoreAction::Unknown;
			}
			DepthStoreAction = mtlpp::StoreAction::Unknown;
			StencilStoreAction = mtlpp::StoreAction::Unknown;
		}
	}
	check(RenderPassDesc);
	
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		FMemory::Memzero(ShaderBuffers[Frequency].Usage);
		ShaderBuffers[Frequency].Bound = 0;
	}
}

void FMetalCommandEncoder::SetRenderPassStoreActions(mtlpp::StoreAction const* const ColorStore, mtlpp::StoreAction const DepthStore, mtlpp::StoreAction const StencilStore)
{
	check(RenderPassDesc);
	{
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
		{
			ColorStoreActions[i] = ColorStore[i];
		}
		DepthStoreAction = DepthStore;
		StencilStoreAction = StencilStore;
	}
}

void FMetalCommandEncoder::SetRenderPipelineState(FMetalShaderPipeline* PipelineState)
{
	check (RenderCommandEncoder);
	{
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetPipeline(PipelineState));
		RenderCommandEncoder.SetRenderPipelineState(PipelineState->RenderPipelineState);
	}
}

void FMetalCommandEncoder::SetViewport(mtlpp::Viewport const Viewport[], uint32 NumActive)
{
	check(RenderCommandEncoder);
	check(NumActive >= 1 && NumActive < ML_MaxViewports);
	if (NumActive == 1)
	{
		RenderCommandEncoder.SetViewport(Viewport[0]);
	}
#if PLATFORM_MAC
	else
	{
		check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports));
		RenderCommandEncoder.SetViewports(Viewport, NumActive);
	}
#endif
}

void FMetalCommandEncoder::SetFrontFacingWinding(mtlpp::Winding const InFrontFacingWinding)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetFrontFacingWinding(InFrontFacingWinding);
	}
}

void FMetalCommandEncoder::SetCullMode(mtlpp::CullMode const InCullMode)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetCullMode(InCullMode);
	}
}

void FMetalCommandEncoder::SetDepthBias(float const InDepthBias, float const InSlopeScale, float const InClamp)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetDepthBias(InDepthBias, InSlopeScale, InClamp);
	}
}

void FMetalCommandEncoder::SetScissorRect(mtlpp::ScissorRect const Rect[], uint32 NumActive)
{
    check(RenderCommandEncoder);
	check(NumActive >= 1 && NumActive < ML_MaxViewports);
	if (NumActive == 1)
	{
		RenderCommandEncoder.SetScissorRect(Rect[0]);
	}
#if PLATFORM_MAC
	else
	{
		check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports));
		RenderCommandEncoder.SetScissorRects(Rect, NumActive);
	}
#endif
}

void FMetalCommandEncoder::SetTriangleFillMode(mtlpp::TriangleFillMode const InFillMode)
{
    check(RenderCommandEncoder);
	{
		RenderCommandEncoder.SetTriangleFillMode(InFillMode);
	}
}

void FMetalCommandEncoder::SetDepthClipMode(mtlpp::DepthClipMode const InDepthClipMode)
{
	check(RenderCommandEncoder);
	{
		RenderCommandEncoder.SetDepthClipMode(InDepthClipMode);
	}
}

void FMetalCommandEncoder::SetBlendColor(float const Red, float const Green, float const Blue, float const Alpha)
{
	check(RenderCommandEncoder);
	{
		RenderCommandEncoder.SetBlendColor(Red, Green, Blue, Alpha);
	}
}

void FMetalCommandEncoder::SetDepthStencilState(mtlpp::DepthStencilState const& InDepthStencilState)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetDepthStencilState(InDepthStencilState);
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetDepthStencilState(InDepthStencilState));
	}
}

void FMetalCommandEncoder::SetStencilReferenceValue(uint32 const ReferenceValue)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetStencilReferenceValue(ReferenceValue);
	}
}

void FMetalCommandEncoder::SetVisibilityResultMode(mtlpp::VisibilityResultMode const Mode, NSUInteger const Offset)
{
    check (RenderCommandEncoder);
	{
		check(Mode == mtlpp::VisibilityResultMode::Disabled || RenderPassDesc.GetVisibilityResultBuffer());
		RenderCommandEncoder.SetVisibilityResultMode(Mode, Offset);
	}
}
	
#pragma mark - Public Shader Resource Mutators -

void FMetalCommandEncoder::SetShaderBuffer(mtlpp::FunctionType const FunctionType, FMetalBuffer const& Buffer, NSUInteger const Offset, NSUInteger const Length, NSUInteger index, mtlpp::ResourceUsage const Usage, EPixelFormat const Format, NSUInteger const ElementRowPitch)
{
	check(index < ML_MaxBuffers);
    if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSetBufferOffset) && Buffer && (ShaderBuffers[uint32(FunctionType)].Bound & (1 << index)) && ShaderBuffers[uint32(FunctionType)].Buffers[index] == Buffer)
    {
		if (FunctionType == mtlpp::FunctionType::Vertex || FunctionType == mtlpp::FunctionType::Kernel)
		{
			FenceResource(Buffer);
		}
		SetShaderBufferOffset(FunctionType, Offset, Length, index);
		ShaderBuffers[uint32(FunctionType)].Usage[index] = Usage;
		ShaderBuffers[uint32(FunctionType)].SetBufferMetaData(index, Length, GMetalBufferFormats[Format].DataFormat, ElementRowPitch);
	}
    else
    {
		if(Buffer)
		{
			ShaderBuffers[uint32(FunctionType)].Bound |= (1 << index);
		}
		else
		{
			ShaderBuffers[uint32(FunctionType)].Bound &= ~(1 << index);
		}
		ShaderBuffers[uint32(FunctionType)].Buffers[index] = Buffer;
		ShaderBuffers[uint32(FunctionType)].Bytes[index] = nil;
		ShaderBuffers[uint32(FunctionType)].Offsets[index] = Offset;
		ShaderBuffers[uint32(FunctionType)].Usage[index] = Usage;
		ShaderBuffers[uint32(FunctionType)].SetBufferMetaData(index, Length, GMetalBufferFormats[Format].DataFormat, ElementRowPitch);
		
		SetShaderBufferInternal(FunctionType, index);
    }
    
	if (Buffer)
	{
		BufferBindingHistory.Add(ns::AutoReleased<FMetalBuffer>(Buffer));
	}
}

void FMetalCommandEncoder::SetShaderData(mtlpp::FunctionType const FunctionType, FMetalBufferData* Data, NSUInteger const Offset, NSUInteger const Index, EPixelFormat const Format, NSUInteger const ElementRowPitch)
{
	check(Index < ML_MaxBuffers);
	
	if(Data)
	{
		ShaderBuffers[uint32(FunctionType)].Bound |= (1 << Index);
	}
	else
	{
		ShaderBuffers[uint32(FunctionType)].Bound &= ~(1 << Index);
	}
	
	ShaderBuffers[uint32(FunctionType)].Buffers[Index] = nil;
	ShaderBuffers[uint32(FunctionType)].Bytes[Index] = Data;
	ShaderBuffers[uint32(FunctionType)].Offsets[Index] = Offset;
	ShaderBuffers[uint32(FunctionType)].Usage[Index] = mtlpp::ResourceUsage::Read;
	ShaderBuffers[uint32(FunctionType)].SetBufferMetaData(Index, Data ? (Data->Len - Offset) : 0, GMetalBufferFormats[Format].DataFormat, ElementRowPitch);
	SetShaderBufferInternal(FunctionType, Index);
}

void FMetalCommandEncoder::SetShaderBytes(mtlpp::FunctionType const FunctionType, uint8 const* Bytes, NSUInteger const Length, NSUInteger const Index)
{
	check(Index < ML_MaxBuffers);
	
	if(Bytes && Length)
	{
		ShaderBuffers[uint32(FunctionType)].Bound |= (1 << Index);

		if (bSupportsMetalFeaturesSetBytes)
		{
			switch (FunctionType)
			{
				case mtlpp::FunctionType::Vertex:
					check(RenderCommandEncoder);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBytes(EMetalShaderVertex, Bytes, Length, Index));
					RenderCommandEncoder.SetVertexData(Bytes, Length, Index);
					break;
				case mtlpp::FunctionType::Fragment:
					check(RenderCommandEncoder);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBytes(EMetalShaderFragment, Bytes, Length, Index));
					RenderCommandEncoder.SetFragmentData(Bytes, Length, Index);
					break;
				case mtlpp::FunctionType::Kernel:
					check(ComputeCommandEncoder);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetBytes(Bytes, Length, Index));
					ComputeCommandEncoder.SetBytes(Bytes, Length, Index);
					break;
				default:
					check(false);
					break;
			}
			
			ShaderBuffers[uint32(FunctionType)].Buffers[Index] = nil;
		}
		else
		{
			FMetalBuffer Buffer = RingBuffer.NewBuffer(Length, BufferOffsetAlignment);
			FMemory::Memcpy(((uint8*)Buffer.GetContents()), Bytes, Length);
			ShaderBuffers[uint32(FunctionType)].Buffers[Index] = Buffer;
		}
		ShaderBuffers[uint32(FunctionType)].Bytes[Index] = nil;
		ShaderBuffers[uint32(FunctionType)].Offsets[Index] = 0;
		ShaderBuffers[uint32(FunctionType)].Usage[Index] = mtlpp::ResourceUsage::Read;
		ShaderBuffers[uint32(FunctionType)].SetBufferMetaData(Index, Length, GMetalBufferFormats[PF_Unknown].DataFormat, 0);
	}
	else
	{
		ShaderBuffers[uint32(FunctionType)].Bound &= ~(1 << Index);
		
		ShaderBuffers[uint32(FunctionType)].Buffers[Index] = nil;
		ShaderBuffers[uint32(FunctionType)].Bytes[Index] = nil;
		ShaderBuffers[uint32(FunctionType)].Offsets[Index] = 0;
		ShaderBuffers[uint32(FunctionType)].Usage[Index] = mtlpp::ResourceUsage(0);
		ShaderBuffers[uint32(FunctionType)].SetBufferMetaData(Index, 0, GMetalBufferFormats[PF_Unknown].DataFormat, 0);
	}
	
	SetShaderBufferInternal(FunctionType, Index);
}

void FMetalCommandEncoder::SetShaderBufferOffset(mtlpp::FunctionType FunctionType, NSUInteger const Offset, NSUInteger const Length, NSUInteger const index)
{
	check(index < ML_MaxBuffers);
    checkf(ShaderBuffers[uint32(FunctionType)].Buffers[index] && (ShaderBuffers[uint32(FunctionType)].Bound & (1 << index)), TEXT("Buffer must already be bound"));
	check(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSetBufferOffset));
	ShaderBuffers[uint32(FunctionType)].Offsets[index] = Offset;
	ShaderBuffers[uint32(FunctionType)].SetBufferMetaData(index, Length, GMetalBufferFormats[PF_Unknown].DataFormat, 0);
	switch (FunctionType)
	{
		case mtlpp::FunctionType::Vertex:
			check (RenderCommandEncoder);
			RenderCommandEncoder.SetVertexBufferOffset(Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBufferOffset(EMetalShaderVertex, Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index));
			break;
		case mtlpp::FunctionType::Fragment:
			check(RenderCommandEncoder);
			RenderCommandEncoder.SetFragmentBufferOffset(Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBufferOffset(EMetalShaderFragment, Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index));
			break;
		case mtlpp::FunctionType::Kernel:
			check (ComputeCommandEncoder);
			ComputeCommandEncoder.SetBufferOffset(Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetBufferOffset(Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index));
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderTexture(mtlpp::FunctionType FunctionType, FMetalTexture const& Texture, NSUInteger index, mtlpp::ResourceUsage Usage)
{
	check(index < ML_MaxTextures);
	switch (FunctionType)
	{
		case mtlpp::FunctionType::Vertex:
			check (RenderCommandEncoder);
			FenceResource(Texture);
			// MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Texture, mtlpp::ResourceUsage::Read));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetTexture(EMetalShaderVertex, Texture, index));
			RenderCommandEncoder.SetVertexTexture(Texture, index);
			break;
		case mtlpp::FunctionType::Fragment:
			check(RenderCommandEncoder);
			// MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Texture, mtlpp::ResourceUsage::Read));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetTexture(EMetalShaderFragment, Texture, index));
			RenderCommandEncoder.SetFragmentTexture(Texture, index);
			break;
		case mtlpp::FunctionType::Kernel:
			check (ComputeCommandEncoder);
			FenceResource(Texture);
			// MTLPP_VALIDATE(mtlpp::ComputeCommandEncoder, ComputeCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Texture, mtlpp::ResourceUsage::Read));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetTexture(Texture, index));
			ComputeCommandEncoder.SetTexture(Texture, index);
			break;
		default:
			check(false);
			break;
	}
	
	if (Texture)
	{
		uint8 Swizzle[4] = {0,0,0,0};
		assert(sizeof(Swizzle) == sizeof(uint32));
		if (Texture.GetPixelFormat() == mtlpp::PixelFormat::X32_Stencil8
#if PLATFORM_MAC
		 ||	Texture.GetPixelFormat() == mtlpp::PixelFormat::X24_Stencil8
#endif
		)
		{
			Swizzle[0] = Swizzle[1] = Swizzle[2] = Swizzle[3] = 1;
		}
		
		ShaderBuffers[uint32(FunctionType)].SetTextureSwizzle(index, Swizzle);
		TextureBindingHistory.Add(ns::AutoReleased<FMetalTexture>(Texture));
	}
}

void FMetalCommandEncoder::SetShaderSamplerState(mtlpp::FunctionType FunctionType, mtlpp::SamplerState const& Sampler, NSUInteger index)
{
	check(index < ML_MaxSamplers);
	switch (FunctionType)
	{
		case mtlpp::FunctionType::Vertex:
       		check (RenderCommandEncoder);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetSamplerState(EMetalShaderVertex, Sampler, index));
			RenderCommandEncoder.SetVertexSamplerState(Sampler, index);
			break;
		case mtlpp::FunctionType::Fragment:
			check (RenderCommandEncoder);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetSamplerState(EMetalShaderFragment, Sampler, index));
			RenderCommandEncoder.SetFragmentSamplerState(Sampler, index);
			break;
		case mtlpp::FunctionType::Kernel:
			check (ComputeCommandEncoder);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetSamplerState(Sampler, index));
			ComputeCommandEncoder.SetSamplerState(Sampler, index);
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderSideTable(mtlpp::FunctionType const FunctionType, NSUInteger const Index)
{
	if (Index < ML_MaxBuffers)
	{
		SetShaderData(FunctionType, ShaderBuffers[uint32(FunctionType)].SideTable, 0, Index);
	}
}

void FMetalCommandEncoder::UseIndirectArgumentResource(FMetalTexture const& Texture, mtlpp::ResourceUsage const Usage)
{
	FenceResource(Texture);
	UseResource(Texture, Usage);
	TextureBindingHistory.Add(ns::AutoReleased<FMetalTexture>(Texture));
}

void FMetalCommandEncoder::UseIndirectArgumentResource(FMetalBuffer const& Buffer, mtlpp::ResourceUsage const Usage)
{
	FenceResource(Buffer);
	UseResource(Buffer, Usage);
	BufferBindingHistory.Add(ns::AutoReleased<FMetalBuffer>(Buffer));
}

void FMetalCommandEncoder::TransitionResources(mtlpp::Resource const& Resource)
{
	TransitionedResources.Add(Resource.GetPtr());
}

#pragma mark - Public Compute State Mutators -

void FMetalCommandEncoder::SetComputePipelineState(FMetalShaderPipeline* State)
{
	check (ComputeCommandEncoder);
	{
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetPipeline(State));
		ComputeCommandEncoder.SetComputePipelineState(State->ComputePipelineState);
	}
}

#pragma mark - Public Ring-Buffer Accessor -
	
FMetalSubBufferRing& FMetalCommandEncoder::GetRingBuffer(void)
{
	return RingBuffer;
}

#pragma mark - Public Resource query Access -

bool FMetalCommandEncoder::HasTextureBindingHistory(FMetalTexture const& Texture) const
{
	return TextureBindingHistory.Contains(ns::AutoReleased<FMetalTexture>(Texture));
}

bool FMetalCommandEncoder::HasBufferBindingHistory(FMetalBuffer const& Buffer) const
{
	return BufferBindingHistory.Contains(ns::AutoReleased<FMetalBuffer>(Buffer));
}

#pragma mark - Private Functions -

void FMetalCommandEncoder::FenceResource(mtlpp::Texture const& Resource)
{
	mtlpp::Resource::Type Res = Resource.GetPtr();
	ns::AutoReleased<mtlpp::Texture> Parent = Resource.GetParentTexture();
	ns::AutoReleased<mtlpp::Buffer> Buffer = Resource.GetBuffer();
	if (Parent)
	{
		Res = Parent.GetPtr();
	}
	else if (Buffer)
	{
		Res = Buffer.GetPtr();
	}
	if (FenceStage == mtlpp::RenderStages::Vertex || FenceResources.Contains(Res))
	{
		FenceStage = mtlpp::RenderStages::Vertex;
		
		for (FMetalFence* FragFence : FragmentFences)
		{
			if (FragFence->NeedsWait(mtlpp::RenderStages::Fragment))
			{
				mtlpp::Fence FragmentFence = FragFence->Get(mtlpp::RenderStages::Fragment);
				mtlpp::Fence FragInnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)FragmentFence.GetPtr()).Inner) :) FragmentFence;
				
				if (RenderCommandEncoder)
				{
					RenderCommandEncoder.WaitForFence(FragInnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex));
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddWaitFence(FragmentFence));
					FragFence->Wait(mtlpp::RenderStages::Fragment);
				}
				else if (ComputeCommandEncoder)
				{
					ComputeCommandEncoder.WaitForFence(FragInnerFence);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddWaitFence(FragmentFence));
					FragFence->Wait(mtlpp::RenderStages::Fragment);
				}
				else if (BlitCommandEncoder)
				{
					BlitCommandEncoder.WaitForFence(FragInnerFence);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddWaitFence(FragmentFence));
					FragFence->Wait(mtlpp::RenderStages::Fragment);
				}
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, WaitCount++);
			}
		}
		FragmentFences.Empty();
	}
}

void FMetalCommandEncoder::FenceResource(mtlpp::Buffer const& Resource)
{
	mtlpp::Resource::Type Res = Resource.GetPtr();
	if (FenceStage == mtlpp::RenderStages::Vertex || FenceResources.Contains(Res))
	{
		FenceStage = mtlpp::RenderStages::Vertex;
		
		for (FMetalFence* FragFence : FragmentFences)
		{
			if (FragFence->NeedsWait(mtlpp::RenderStages::Fragment))
			{
				mtlpp::Fence FragmentFence = FragFence->Get(mtlpp::RenderStages::Fragment);
				mtlpp::Fence FragInnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)FragmentFence.GetPtr()).Inner) :) FragmentFence;
				
				if (RenderCommandEncoder)
				{
					RenderCommandEncoder.WaitForFence(FragInnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex));
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddWaitFence(FragmentFence));
					FragFence->Wait(mtlpp::RenderStages::Fragment);
				}
				else if (ComputeCommandEncoder)
				{
					ComputeCommandEncoder.WaitForFence(FragInnerFence);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddWaitFence(FragmentFence));
					FragFence->Wait(mtlpp::RenderStages::Fragment);
				}
				else if (BlitCommandEncoder)
				{
					BlitCommandEncoder.WaitForFence(FragInnerFence);
					METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddWaitFence(FragmentFence));
					FragFence->Wait(mtlpp::RenderStages::Fragment);
				}
			}
		}
		FragmentFences.Empty();
	}
}

void FMetalCommandEncoder::UseResource(mtlpp::Resource const& Resource, mtlpp::ResourceUsage const Usage)
{
	static bool UseResourceAvailable = FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs);
	if (UseResourceAvailable || SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
	{
		mtlpp::ResourceUsage Current = ResourceUsage.FindRef(Resource.GetPtr());
		if (Current != Usage)
		{
			ResourceUsage.Add(Resource.GetPtr(), Usage);
			if (RenderCommandEncoder)
			{
				MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Resource, Usage));
			}
			else if (ComputeCommandEncoder)
			{
				MTLPP_VALIDATE(mtlpp::ComputeCommandEncoder, ComputeCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Resource, Usage));
			}
		}
	}
}

void FMetalCommandEncoder::SetShaderBufferInternal(mtlpp::FunctionType Function, uint32 Index)
{
	FMetalBufferBindings& Binding = ShaderBuffers[uint32(Function)];

	NSUInteger Offset = Binding.Offsets[Index];
	
	bool bBufferHasBytes = Binding.Bytes[Index] != nil;
	if (!Binding.Buffers[Index] && bBufferHasBytes && !bSupportsMetalFeaturesSetBytes)
	{
		uint8 const* Bytes = (((uint8 const*)Binding.Bytes[Index]->Data) + Binding.Offsets[Index]);
		uint32 Len = Binding.Bytes[Index]->Len - Binding.Offsets[Index];
		
		Offset = 0;
		Binding.Buffers[Index] = RingBuffer.NewBuffer(Len, BufferOffsetAlignment);
		
		FMemory::Memcpy(((uint8*)Binding.Buffers[Index].GetContents()) + Offset, Bytes, Len);
	}
	
	ns::AutoReleased<FMetalBuffer>& Buffer = Binding.Buffers[Index];
	if (Buffer)
	{
#if METAL_DEBUG_OPTIONS
		if(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
		{
			ActiveBuffers.Add(Buffer);
		}
#endif
        
		switch (Function)
		{
			case mtlpp::FunctionType::Vertex:
				Binding.Bound |= (1 << Index);
				check(RenderCommandEncoder);
				FenceResource(Buffer);
				// MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Buffer, mtlpp::ResourceUsage::Read));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBuffer(EMetalShaderVertex, Buffer, Offset, Index));
				RenderCommandEncoder.SetVertexBuffer(Buffer, Offset, Index);
				break;

			case mtlpp::FunctionType::Fragment:
				Binding.Bound |= (1 << Index);
				check(RenderCommandEncoder);
				// MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Buffer, mtlpp::ResourceUsage::Read));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBuffer(EMetalShaderFragment, Buffer, Offset, Index));
				RenderCommandEncoder.SetFragmentBuffer(Buffer, Offset, Index);
				break;

			case mtlpp::FunctionType::Kernel:
				Binding.Bound |= (1 << Index);
				check(ComputeCommandEncoder);
				FenceResource(Buffer);
				// MTLPP_VALIDATE(mtlpp::ComputeCommandEncoder, ComputeCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Buffer, mtlpp::ResourceUsage::Read));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetBuffer(Buffer, Offset, Index));
				ComputeCommandEncoder.SetBuffer(Buffer, Offset, Index);
				break;

			default:
				check(false);
				break;
		}
		
		if (Buffer.IsSingleUse())
		{
			Binding.Usage[Index] = mtlpp::ResourceUsage(0);
			Binding.Offsets[Index] = 0;
			Binding.Buffers[Index] = nil;
			Binding.Bound &= ~(1 << Index);
		}
	}
	else if (bBufferHasBytes && bSupportsMetalFeaturesSetBytes)
	{
		uint8 const* Bytes = (((uint8 const*)Binding.Bytes[Index]->Data) + Binding.Offsets[Index]);
		uint32 Len = Binding.Bytes[Index]->Len - Binding.Offsets[Index];
		
		switch (Function)
		{
			case mtlpp::FunctionType::Vertex:
				Binding.Bound |= (1 << Index);
				check(RenderCommandEncoder);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBytes(EMetalShaderVertex, Bytes, Len, Index));
				RenderCommandEncoder.SetVertexData(Bytes, Len, Index);
				break;

			case mtlpp::FunctionType::Fragment:
				Binding.Bound |= (1 << Index);
				check(RenderCommandEncoder);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBytes(EMetalShaderFragment, Bytes, Len, Index));
				RenderCommandEncoder.SetFragmentData(Bytes, Len, Index);
				break;

			case mtlpp::FunctionType::Kernel:
				Binding.Bound |= (1 << Index);
				check(ComputeCommandEncoder);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetBytes(Bytes, Len, Index));
				ComputeCommandEncoder.SetBytes(Bytes, Len, Index);
				break;

			default:
				check(false);
				break;
		}
	}
}
