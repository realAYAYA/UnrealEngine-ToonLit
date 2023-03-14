// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandList.cpp: Metal command buffer list wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalShaderTypes.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalCommandList.h"
#include "MetalCommandQueue.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandEncoder.h"
#include "ns.hpp"

#pragma mark - Public C++ Boilerplate -

#if PLATFORM_IOS
extern bool GIsSuspended;
#endif
extern int32 GMetalDebugOpsCount;

FMetalCommandList::FMetalCommandList(FMetalCommandQueue& InCommandQueue, bool const bInImmediate)
: CommandQueue(InCommandQueue)
, Index(0)
, Num(0)
, bImmediate(bInImmediate)
{
}

FMetalCommandList::~FMetalCommandList(void)
{
}
	
#pragma mark - Public Command List Mutators -

extern CORE_API bool GIsGPUCrashed;
static void ReportMetalCommandBufferFailure(mtlpp::CommandBuffer const& CompletedBuffer, TCHAR const* ErrorType, bool bDoCheck=true)
{
	GIsGPUCrashed = true;
	
	NSString* Label = CompletedBuffer.GetLabel();
	int32 Code = CompletedBuffer.GetError().GetCode();
	NSString* Domain = CompletedBuffer.GetError().GetDomain();
	NSString* ErrorDesc = CompletedBuffer.GetError().GetLocalizedDescription();
	NSString* FailureDesc = CompletedBuffer.GetError().GetLocalizedFailureReason();
	NSString* RecoveryDesc = CompletedBuffer.GetError().GetLocalizedRecoverySuggestion();
	
	FString LabelString = Label ? FString(Label) : FString(TEXT("Unknown"));
	FString DomainString = Domain ? FString(Domain) : FString(TEXT("Unknown"));
	FString ErrorString = ErrorDesc ? FString(ErrorDesc) : FString(TEXT("Unknown"));
	FString FailureString = FailureDesc ? FString(FailureDesc) : FString(TEXT("Unknown"));
	FString RecoveryString = RecoveryDesc ? FString(RecoveryDesc) : FString(TEXT("Unknown"));
	
	NSString* Desc = CompletedBuffer.GetPtr().debugDescription;
	UE_LOG(LogMetal, Warning, TEXT("%s"), *FString(Desc));
	
#if PLATFORM_IOS
    if (bDoCheck && !GIsSuspended && !GIsRenderingThreadSuspended)
#endif
    {
		FMetalCommandBufferMarkers Markers = FMetalCommandBufferMarkers::Get(CompletedBuffer);
		if (Markers)
		{
			uint32 CommandBufferIndex = Markers.GetIndex();
			FMetalDebugInfo BrokenDraw;
			BrokenDraw.EncoderIndex = UINT32_MAX;
			BrokenDraw.CommandIndex = UINT32_MAX;
			BrokenDraw.CmdBuffIndex = UINT32_MAX;
			BrokenDraw.ContextIndex = UINT32_MAX;
            BrokenDraw.CommandBuffer = 0;
			uint32 BrokenContext = 0;
			bool bFoundBrokenDraw = false;
			for (uint32 i = 0; (CommandBufferIndex != ~0u) && !bFoundBrokenDraw && i < Markers.NumContexts(); i++)
			{
				ns::AutoReleased<FMetalBuffer> DebugBuffer = Markers.GetDebugBuffer(i);
				TArray<FMetalCommandDebug>* Commands = Markers.GetCommands(i);
				if (DebugBuffer && Commands && Commands->Num())
				{
					uint32 DebugLength = DebugBuffer.GetLength();
					uint32 DebugCount = DebugLength / sizeof(FMetalDebugInfo);
					check(DebugCount >= 1);
					
					FMetalDebugInfo* DebugArray = (FMetalDebugInfo*)DebugBuffer.GetContents();
					FMetalDebugInfo CurrentDraw = DebugArray[0];
					
					// On TBDR we find the disjoint where one tile has progressed further than another
					// We find the earliest failure in the probably vain hope that this is the tile that actually failed
					// There's actually no guarantee of that, it might be one of the later tiles that exploded
					for (uint32 j = 0; j < DebugCount; j++)
					{
						if (CommandBufferIndex == DebugArray[j].CmdBuffIndex && DebugArray[j].EncoderIndex < CurrentDraw.EncoderIndex && DebugArray[j].CommandIndex < CurrentDraw.CommandIndex)
						{
							CurrentDraw = DebugArray[j];
							break;
						}
					}

					// Find the first command to fail - which depends on the order of encoders - parallel contexts make this more complicated
					if (CurrentDraw.CmdBuffIndex == BrokenDraw.CmdBuffIndex && CurrentDraw.CommandIndex < Commands->Num() && CurrentDraw.EncoderIndex < BrokenDraw.EncoderIndex)
					{
						BrokenDraw = CurrentDraw;
                        BrokenContext = i;
                        bFoundBrokenDraw = true;
					}
                    else if(Commands->Num() > 1)
                    {
						BrokenDraw.EncoderIndex = 0;
						BrokenDraw.CommandIndex = 0;
						BrokenDraw.CmdBuffIndex = CommandBufferIndex;
						BrokenDraw.ContextIndex = 0;
                        BrokenContext = i;
						BrokenDraw.CommandBuffer = (uintptr_t)CompletedBuffer.GetPtr();
                        BrokenDraw = DebugArray[1];
                        bFoundBrokenDraw = true;
                    }
				}
			}
			if (bFoundBrokenDraw)
			{
				id<MTLCommandBuffer> Ptr = reinterpret_cast<id<MTLCommandBuffer>>(BrokenDraw.CommandBuffer);
				UE_LOG(LogMetal, Error, TEXT("GPU last wrote Command Buffer: %u (%llx) Encoder Index: %d Context Index: %d Draw Index: %d PSO: VS: %u_%u, PS: %u_%u."), BrokenDraw.CmdBuffIndex, BrokenDraw.CommandBuffer, BrokenDraw.EncoderIndex, BrokenDraw.ContextIndex, BrokenDraw.CommandIndex, BrokenDraw.PSOSignature[0], BrokenDraw.PSOSignature[1], BrokenDraw.PSOSignature[2], BrokenDraw.PSOSignature[3]);
			
				ns::AutoReleased<FMetalBuffer> DebugBuffer = Markers.GetDebugBuffer(BrokenContext);
				TArray<FMetalCommandDebug>* Commands = Markers.GetCommands(BrokenContext);
				if (Commands->Num())
				{
					UE_LOG(LogMetal, Error, TEXT("Failed executing following commands:"));
					uint32 StartIdx = BrokenDraw.CommandIndex;
					for (uint32 CmdIdx = StartIdx; CmdIdx < Commands->Num(); CmdIdx++)
					{
						FMetalCommandDebug& Command = (*Commands)[CmdIdx];
						FString VSHash = Command.PSO->VertexShader->GetHash().ToString();
						uint32 VSSig[2] = { Command.PSO->VertexShader->SourceLen, Command.PSO->VertexShader->SourceCRC };
						
						FString PSHash;
						uint32 PSSig[2] = { 0, 0 };
						if (Command.PSO->PixelShader.IsValid())
						{
							PSHash = Command.PSO->PixelShader->GetHash().ToString();
							PSSig[0] = Command.PSO->PixelShader->SourceLen;
							PSSig[1] = Command.PSO->PixelShader->SourceCRC;
						}
						
						UE_LOG(LogMetal, Error, TEXT("Command Buffer: %d (%p) Encoder: %d Command: %d: %s PSO: VS: %s (%u_%u), PS: %s (%u_%u)"), Command.CmdBufIndex, CompletedBuffer.GetPtr(), Command.Encoder, Command.Index, *Command.Data.ToString(), *VSHash, VSSig[0], VSSig[1], *PSHash, PSSig[0], PSSig[1]);
					}
				}
                if (DebugBuffer)
                {
                    uint32 DebugLength = DebugBuffer.GetLength();
                    uint32 DebugCount = DebugLength / sizeof(FMetalDebugInfo);
                    check(DebugCount >= 1);
                    
                    FMetalDebugInfo* DebugArray = (FMetalDebugInfo*)DebugBuffer.GetContents();
                    for (uint32 i = 0; i < DebugCount; i++)
                    {
                        FMetalDebugInfo& Command = DebugArray[i];
						
						// Stop when nothing has been written into the buffer
						if (Command.CmdBuffIndex == 0 && Command.CommandBuffer == 0 && Command.EncoderIndex == 0 && Command.CommandIndex == 0 && Command.PSOSignature[0] == 0 && Command.PSOSignature[1] == 0 && Command.PSOSignature[2] == 0 && Command.PSOSignature[3] == 0)
						{
							break;
						}
						
                        UE_LOG(LogMetal, Error, TEXT("Command Buffer: %d (%p) Debug Buffer: %p Tile: %u Context: %d Encoder: %d Command: %d PSO: VS: %u_%u, PS: %u_%u"), Command.CmdBuffIndex, Command.CommandBuffer, DebugBuffer.GetPtr(), i, Command.ContextIndex, Command.EncoderIndex, Command.CommandIndex, Command.PSOSignature[0], Command.PSOSignature[1], Command.PSOSignature[2], Command.PSOSignature[3]);
                    }
                }
			}
		}
		
#if PLATFORM_IOS
        UE_LOG(LogMetal, Warning, TEXT("Command Buffer %s Failed with %s Error! Error Domain: %s Code: %d Description %s %s %s"), *LabelString, ErrorType, *DomainString, Code, *ErrorString, *FailureString, *RecoveryString);
        FIOSPlatformMisc::GPUAssert();
#else
		UE_LOG(LogMetal, Fatal, TEXT("Command Buffer %s Failed with %s Error! Error Domain: %s Code: %d Description %s %s %s"), *LabelString, ErrorType, *DomainString, Code, *ErrorString, *FailureString, *RecoveryString);
#endif
    }
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureInternal(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Internal"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureTimeout(mtlpp::CommandBuffer const& CompletedBuffer)
{
    ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Timeout"), PLATFORM_IOS);
}

static __attribute__ ((optnone)) void MetalCommandBufferFailurePageFault(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("PageFault"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureAccessRevoked(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("AccessRevoked"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureNotPermitted(mtlpp::CommandBuffer const& CompletedBuffer)
{
	// when iOS goes into the background, it can get a delayed NotPermitted error, so we can't crash in this case, just allow it to not be submitted
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("NotPermitted"), !PLATFORM_IOS);
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureOutOfMemory(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("OutOfMemory"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureInvalidResource(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("InvalidResource"));
}

static void HandleMetalCommandBufferError(mtlpp::CommandBuffer const& CompletedBuffer)
{
	MTLCommandBufferError Code = (MTLCommandBufferError)CompletedBuffer.GetError().GetCode();
	switch(Code)
	{
		case MTLCommandBufferErrorInternal:
			MetalCommandBufferFailureInternal(CompletedBuffer);
			break;
		case MTLCommandBufferErrorTimeout:
			MetalCommandBufferFailureTimeout(CompletedBuffer);
			break;
		case MTLCommandBufferErrorPageFault:
			MetalCommandBufferFailurePageFault(CompletedBuffer);
			break;
		case MTLCommandBufferErrorAccessRevoked:
			MetalCommandBufferFailureAccessRevoked(CompletedBuffer);
			break;
		case MTLCommandBufferErrorNotPermitted:
			MetalCommandBufferFailureNotPermitted(CompletedBuffer);
			break;
		case MTLCommandBufferErrorOutOfMemory:
			MetalCommandBufferFailureOutOfMemory(CompletedBuffer);
			break;
		case MTLCommandBufferErrorInvalidResource:
			MetalCommandBufferFailureInvalidResource(CompletedBuffer);
			break;
		case MTLCommandBufferErrorNone:
			// No error
			break;
		default:
			ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Unknown"));
			break;
	}
}

static __attribute__ ((optnone)) void HandleAMDMetalCommandBufferError(mtlpp::CommandBuffer const& CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

static __attribute__ ((optnone)) void HandleNVIDIAMetalCommandBufferError(mtlpp::CommandBuffer const& CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

static __attribute__ ((optnone)) void HandleIntelMetalCommandBufferError(mtlpp::CommandBuffer const& CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

void FMetalCommandList::HandleMetalCommandBufferFailure(mtlpp::CommandBuffer const& CompletedBuffer)
{
	if (CompletedBuffer.GetError().GetDomain() == MTLCommandBufferErrorDomain || [CompletedBuffer.GetError().GetDomain() isEqualToString:MTLCommandBufferErrorDomain])
	{
		if (GRHIVendorId && IsRHIDeviceAMD())
		{
			HandleAMDMetalCommandBufferError(CompletedBuffer);
		}
		else if (GRHIVendorId && IsRHIDeviceNVIDIA())
		{
			HandleNVIDIAMetalCommandBufferError(CompletedBuffer);
		}
		else if (GRHIVendorId && IsRHIDeviceIntel())
		{
			HandleIntelMetalCommandBufferError(CompletedBuffer);
		}
		else
		{
			HandleMetalCommandBufferError(CompletedBuffer);
		}
	}
	else
	{
		ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Unknown"));
	}
}

void FMetalCommandList::SetParallelIndex(uint32 InIndex, uint32 InNum)
{
	if (!IsImmediate())
	{
		Index = InIndex;
		Num = InNum;
	}
}

void FMetalCommandList::Commit(mtlpp::CommandBuffer& Buffer, TArray<ns::Object<mtlpp::CommandBufferHandler>> CompletionHandlers, bool const bWait, bool const bIsLastCommandBuffer)
{
	check(Buffer);

	// The lifetime of this array is per frame
	if (!FrameCommitedBufferTimings.IsValid())
	{
		FrameCommitedBufferTimings = MakeShared<TArray<FMetalCommandBufferTiming>, ESPMode::ThreadSafe>();
	}

	// The lifetime of this should be for the entire game
	if (!LastCompletedBufferTiming.IsValid())
	{
		LastCompletedBufferTiming = MakeShared<FMetalCommandBufferTiming, ESPMode::ThreadSafe>();
	}

	Buffer.AddCompletedHandler([CompletionHandlers, FrameCommitedBufferTimingsLocal = FrameCommitedBufferTimings, LastCompletedBufferTimingLocal = LastCompletedBufferTiming](mtlpp::CommandBuffer const& CompletedBuffer)
	{
		if (CompletedBuffer.GetStatus() == mtlpp::CommandBufferStatus::Error)
		{
			HandleMetalCommandBufferFailure(CompletedBuffer);
		}
		if (CompletionHandlers.Num())
		{
			for (ns::Object<mtlpp::CommandBufferHandler> Handler : CompletionHandlers)
			{
				Handler.GetPtr()(CompletedBuffer);
			}
		}

		if (CompletedBuffer.GetStatus() == mtlpp::CommandBufferStatus::Completed)
		{
			FrameCommitedBufferTimingsLocal->Add({CompletedBuffer.GetGpuStartTime(), CompletedBuffer.GetGpuEndTime()});
		}

		// If this is the last reference, then it is the last command buffer to return, so record the frame
		if (FrameCommitedBufferTimingsLocal.IsUnique())
		{
			FMetalGPUProfiler::RecordFrame(*FrameCommitedBufferTimingsLocal, *LastCompletedBufferTimingLocal);
		}
	});

	// If bIsLastCommandBuffer is set then this is the end of the "frame".
	if (bIsLastCommandBuffer)
	{
		FrameCommitedBufferTimings = MakeShared<TArray<FMetalCommandBufferTiming>, ESPMode::ThreadSafe>();
	}

	if (bImmediate)
	{
		CommandQueue.CommitCommandBuffer(Buffer);
		if (bWait)
		{
			Buffer.WaitUntilCompleted();
		}
	}
	else
	{
		check(!bWait);
		SubmittedBuffers.Add(Buffer);
	}
}

void FMetalCommandList::Submit(uint32 InIndex, uint32 Count)
{
	// Only deferred contexts should call Submit, the immediate context commits directly to the command-queue.
	check(!bImmediate);

	// Command queue takes ownership of the array
	CommandQueue.SubmitCommandBuffers(SubmittedBuffers, InIndex, Count);
	SubmittedBuffers.Empty();
}
