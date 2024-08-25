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

#pragma mark - Public C++ Boilerplate -

#if PLATFORM_IOS
extern bool GIsSuspended;
#endif

FMetalCommandList::FMetalCommandList(FMetalCommandQueue& InCommandQueue)
	: CommandQueue(InCommandQueue)
{}

FMetalCommandList::~FMetalCommandList(void)
{
}
	
#pragma mark - Public Command List Mutators -

static const TCHAR* StringFromCommandEncoderError(MTL::CommandEncoderErrorState ErrorState)
{
    switch (ErrorState)
    {
        case MTL::CommandEncoderErrorStateUnknown: return TEXT("Unknown");
        case MTL::CommandEncoderErrorStateAffected: return TEXT("Affected");
        case MTL::CommandEncoderErrorStateCompleted: return TEXT("Completed");
        case MTL::CommandEncoderErrorStateFaulted: return TEXT("Faulted");
        case MTL::CommandEncoderErrorStatePending: return TEXT("Pending");
    }
    return TEXT("Unknown");
}

extern CORE_API bool GIsGPUCrashed;
static void ReportMetalCommandBufferFailure(MTL::CommandBuffer* CompletedBuffer, TCHAR const* ErrorType, bool bDoCheck=true)
{
	GIsGPUCrashed = true;
	
	NS::String* Label = CompletedBuffer->label();
	int32 Code = CompletedBuffer->error()->code();
    NS::String* Domain = CompletedBuffer->error()->domain();
    NS::String* ErrorDesc = CompletedBuffer->error()->localizedDescription();
    NS::String* FailureDesc = CompletedBuffer->error()->localizedFailureReason();
    NS::String* RecoveryDesc = CompletedBuffer->error()->localizedRecoverySuggestion();
	
	FString LabelString = Label ? FString(Label->cString(NS::UTF8StringEncoding)) : FString(TEXT("Unknown"));
	FString DomainString = Domain ? FString(Domain->cString(NS::UTF8StringEncoding)) : FString(TEXT("Unknown"));
	FString ErrorString = ErrorDesc ? FString(ErrorDesc->cString(NS::UTF8StringEncoding)) : FString(TEXT("Unknown"));
	FString FailureString = FailureDesc ? FString(FailureDesc->cString(NS::UTF8StringEncoding)) : FString(TEXT("Unknown"));
	FString RecoveryString = RecoveryDesc ? FString(RecoveryDesc->cString(NS::UTF8StringEncoding)) : FString(TEXT("Unknown"));
	
	NS::String* Desc = CompletedBuffer->debugDescription();
	UE_LOG(LogMetal, Warning, TEXT("%s"), *FString(Desc->cString(NS::UTF8StringEncoding)));
	
#if PLATFORM_IOS
    if (bDoCheck && !GIsSuspended && !GIsRenderingThreadSuspended)
#endif
    {
        // Dump GPU fault information for the GPU encoders
        if (&MTLCommandBufferEncoderInfoErrorKey != nullptr)
        {
            NS::Dictionary* ErrorDict = CompletedBuffer->error()->userInfo();
            NS::Array* EncoderInfoArray = (NS::Array*)ErrorDict->object(MTL::CommandBufferEncoderInfoErrorKey);
            if (EncoderInfoArray)
            {
                UE_LOG(LogMetal, Warning, TEXT("GPU Encoder Crash Info:"));
                for(uint32 Idx = 0; Idx < EncoderInfoArray->count(); ++Idx)
                {
                    MTL::CommandBufferEncoderInfo* EncoderInfo = (MTL::CommandBufferEncoderInfo*)EncoderInfoArray->object(Idx);
                    UE_LOG(LogMetal, Warning, TEXT("MTLCommandBufferEncoder - Label: %s, State: %s"), *NSStringToFString(EncoderInfo->label()), StringFromCommandEncoderError(EncoderInfo->errorState()));
                    NS::Array* SignPosts = EncoderInfo->debugSignposts();
                    if (SignPosts->count() > 0)
                    {
                        UE_LOG(LogMetal, Warning, TEXT("    Signposts:"));
                        for (uint32_t SignPostIdx = 0; SignPostIdx < SignPosts->count(); ++SignPostIdx)
                        {
                            NS::String* Signpost = (NS::String*)SignPosts->object(SignPostIdx);
                            UE_LOG(LogMetal, Warning, TEXT("    - %s"), *NSStringToFString(Signpost));
                        }
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

static __attribute__ ((optnone)) void MetalCommandBufferFailureInternal(MTL::CommandBuffer* CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Internal"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureTimeout(MTL::CommandBuffer* CompletedBuffer)
{
    ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Timeout"), PLATFORM_IOS);
}

static __attribute__ ((optnone)) void MetalCommandBufferFailurePageFault(MTL::CommandBuffer* CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("PageFault"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureAccessRevoked(MTL::CommandBuffer* CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("AccessRevoked"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureNotPermitted(MTL::CommandBuffer* CompletedBuffer)
{
	// when iOS goes into the background, it can get a delayed NotPermitted error, so we can't crash in this case, just allow it to not be submitted
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("NotPermitted"), !PLATFORM_IOS);
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureOutOfMemory(MTL::CommandBuffer* CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("OutOfMemory"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureInvalidResource(MTL::CommandBuffer* CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("InvalidResource"));
}

static void HandleMetalCommandBufferError(MTL::CommandBuffer* CompletedBuffer)
{
    MTL::CommandBufferError Code = (MTL::CommandBufferError)CompletedBuffer->error()->code();
	switch(Code)
	{
        case MTL::CommandBufferErrorInternal:
			MetalCommandBufferFailureInternal(CompletedBuffer);
			break;
        case MTL::CommandBufferErrorTimeout:
			MetalCommandBufferFailureTimeout(CompletedBuffer);
			break;
        case MTL::CommandBufferErrorPageFault:
			MetalCommandBufferFailurePageFault(CompletedBuffer);
			break;
        case MTL::CommandBufferErrorAccessRevoked:
			MetalCommandBufferFailureAccessRevoked(CompletedBuffer);
			break;
        case MTL::CommandBufferErrorNotPermitted:
			MetalCommandBufferFailureNotPermitted(CompletedBuffer);
			break;
        case MTL::CommandBufferErrorOutOfMemory:
			MetalCommandBufferFailureOutOfMemory(CompletedBuffer);
			break;
        case MTL::CommandBufferErrorInvalidResource:
			MetalCommandBufferFailureInvalidResource(CompletedBuffer);
			break;
        case MTL::CommandBufferErrorNone:
			// No error
			break;
		default:
			ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Unknown"));
			break;
	}
}

static __attribute__ ((optnone)) void HandleAMDMetalCommandBufferError(MTL::CommandBuffer* CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

static __attribute__ ((optnone)) void HandleIntelMetalCommandBufferError(MTL::CommandBuffer* CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

void FMetalCommandList::HandleMetalCommandBufferFailure(MTL::CommandBuffer* CompletedBuffer)
{
	if (CompletedBuffer->error()->domain()->isEqualToString(NS::String::string("MTLCommandBufferErrorDomain", NS::UTF8StringEncoding)))
	{
		if (GRHIVendorId && IsRHIDeviceAMD())
		{
			HandleAMDMetalCommandBufferError(CompletedBuffer);
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

void FMetalCommandList::Commit(FMetalCommandBuffer* Buffer, TArray<FMetalCommandBufferCompletionHandler> CompletionHandlers, bool const bWait, bool const bIsLastCommandBuffer)
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
    
    MTL::HandlerFunction CompletionHandler = [CompletionHandlers, FrameCommitedBufferTimingsLocal = FrameCommitedBufferTimings, LastCompletedBufferTimingLocal = LastCompletedBufferTiming, Buffer](MTL::CommandBuffer* CompletedBuffer)
	{
		if (CompletedBuffer->status() == MTL::CommandBufferStatusError)
		{
			HandleMetalCommandBufferFailure(CompletedBuffer);
		}
		if (CompletionHandlers.Num())
		{
			for (FMetalCommandBufferCompletionHandler Handler : CompletionHandlers)
			{
				Handler.Execute(CompletedBuffer);
			}
		}

		if (CompletedBuffer->status() == MTL::CommandBufferStatusCompleted)
		{
			FrameCommitedBufferTimingsLocal->Add({CompletedBuffer->GPUStartTime(), CompletedBuffer->GPUEndTime()});
		}

		// If this is the last reference, then it is the last command buffer to return, so record the frame
		if (FrameCommitedBufferTimingsLocal.IsUnique())
		{
			FMetalGPUProfiler::RecordFrame(*FrameCommitedBufferTimingsLocal, *LastCompletedBufferTimingLocal);
		}
	};
    
    Buffer->GetMTLCmdBuffer()->addCompletedHandler(CompletionHandler);
    
	// If bIsLastCommandBuffer is set then this is the end of the "frame".
	if (bIsLastCommandBuffer)
	{
		FrameCommitedBufferTimings = MakeShared<TArray<FMetalCommandBufferTiming>, ESPMode::ThreadSafe>();
	}
    
	CommandQueue.CommitCommandBuffer(Buffer);
	if (bWait)
	{
		Buffer->GetMTLCmdBuffer()->waitUntilCompleted();
	}
}
