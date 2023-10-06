// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxFrameManager.h"

#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "RivermaxLog.h"
#include "RivermaxTracingUtils.h"

namespace UE::RivermaxCore::Private
{
	/** Sidecar used when initiating memcopy. We provide the frame involved to update its state. */
	struct FFrameBufferCopyInfo : public FBaseDataCopySideCar
	{
		TSharedPtr<FRivermaxOutputFrame> CopiedFrame;
	};

	FFrameManager::~FFrameManager()
	{
		Cleanup();
	}

	EFrameMemoryLocation FFrameManager::Initialize(const FFrameManagerSetupArgs& Args)
	{
		RivermaxManager = FModuleManager::GetModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore")).GetRivermaxManager();
		check(RivermaxManager);

		OnFrameReadyDelegate = Args.OnFrameReadyDelegate;
		OnPreFrameReadyDelegate = Args.OnPreFrameReadyDelegate;
		OnFreeFrameDelegate = Args.OnFreeFrameDelegate;
		OnCriticalErrorDelegate = Args.OnCriticalErrorDelegate;
		FrameResolution = Args.Resolution;
		TotalFrameCount = Args.NumberOfFrames;
		FOnFrameDataCopiedDelegate OnDataCopiedDelegate = FOnFrameDataCopiedDelegate::CreateRaw(this, &FFrameManager::OnDataCopied);

		if (Args.bTryGPUAllocation)
		{
			FrameAllocator = MakeUnique<FGPUAllocator>(Args.FrameDesiredSize, OnDataCopiedDelegate);
			if (FrameAllocator->Allocate(TotalFrameCount, Args.bAlignEachFrameAlloc))
			{
				MemoryLocation = EFrameMemoryLocation::GPU;
			}
		}

		if (MemoryLocation == EFrameMemoryLocation::None)
		{
			FrameAllocator = MakeUnique<FSystemAllocator>(Args.FrameDesiredSize, OnDataCopiedDelegate);
			if (FrameAllocator->Allocate(TotalFrameCount, Args.bAlignEachFrameAlloc))
			{
				MemoryLocation = EFrameMemoryLocation::System;
			}
		}

		if (MemoryLocation != EFrameMemoryLocation::None)
		{
			// Create frame state tracking containers
			FreeFrames.Reserve(TotalFrameCount);
			PendingFrames.Reserve(TotalFrameCount);
			ReadyFrames.Reserve(TotalFrameCount);
			for (uint32 Index = 0; Index < TotalFrameCount; ++Index)
			{
				// All frames default to being available
				FreeFrames.Add(Index);

				/** Create actual frames and assign their video memory address from allocator */
				TSharedPtr<FRivermaxOutputFrame> Frame = MakeShared<FRivermaxOutputFrame>(Index);
				Frame->VideoBuffer = FrameAllocator->GetFrameAddress(Index);
				Frames.Add(MoveTemp(Frame));
			}
		}

		return MemoryLocation;
	}

	void FFrameManager::Cleanup()
	{
		if (FrameAllocator)
		{
			FrameAllocator->Deallocate();
			FrameAllocator.Reset();
		}
	}

	TSharedPtr<UE::RivermaxCore::Private::FRivermaxOutputFrame> FFrameManager::GetFreeFrame()
	{
		FScopeLock Lock(&ContainersCritSec);
		if (!FreeFrames.IsEmpty())
		{
			return Frames[FreeFrames[0]];
		}

		return nullptr;
	}

	void FFrameManager::MarkAsPending(const TSharedPtr<FRivermaxOutputFrame>& Frame)
	{
		FScopeLock Lock(&ContainersCritSec);

		ensure(FreeFrames.RemoveSingle(Frame->FrameIndex) >= 1);
		PendingFrames.Add(Frame->FrameIndex);
	}

	TSharedPtr<UE::RivermaxCore::Private::FRivermaxOutputFrame> FFrameManager::GetNextFrame(uint32 NextFrameIdentifier)
	{
		FScopeLock Lock(&ContainersCritSec);
		
		TSharedPtr<FRivermaxOutputFrame> NextFrame = GetPendingFrame(NextFrameIdentifier);
		if (NextFrame)
		{
			// We found a pending / reserved frame matching identifier
			return NextFrame;
		}

		//Otherwise, prepare next free frame
		NextFrame = GetFreeFrame();
		if (NextFrame)
		{
			NextFrame->Reset();
			NextFrame->FrameIdentifier = NextFrameIdentifier;
			MarkAsPending(NextFrame);
		}

		return NextFrame;
	}

	TSharedPtr<FRivermaxOutputFrame> FFrameManager::GetPendingFrame(uint32 FrameIdentifier)
	{
		FScopeLock Lock(&ContainersCritSec);
		if (!PendingFrames.IsEmpty())
		{
			for (const uint32 FrameIndex : PendingFrames)
			{
				TSharedPtr<FRivermaxOutputFrame> Frame = Frames[FrameIndex];
				if (Frame->FrameIdentifier == FrameIdentifier)
				{
					// We found a reserved / pending frame corresponding to next frame identifier
					return Frame;
				}
			}
		}
		return nullptr;
	}

	TSharedPtr<FRivermaxOutputFrame> FFrameManager::GetReadyFrame()
	{
		FScopeLock Lock(&ContainersCritSec);
		if (!ReadyFrames.IsEmpty())
		{
			const uint32 NextReadyFrame = ReadyFrames[0];
			return Frames[NextReadyFrame];
		}
	
		return nullptr;
	}

	void FFrameManager::MarkAsSent(const TSharedPtr<FRivermaxOutputFrame>& Frame)
	{
		{
			FScopeLock Lock(&ContainersCritSec);

			Frame->Reset();
			FreeFrames.Add(SendingFrame);

			if (ensure(Frame->FrameIndex == SendingFrame))
			{
				SendingFrame = INDEX_NONE;
			}
		}
		
		OnFreeFrameDelegate.ExecuteIfBound();
	}

	void FFrameManager::MarkAsReady(const TSharedPtr<FRivermaxOutputFrame>& Frame)
	{
		{
			// Make frame available to be sent
			FScopeLock Lock(&ContainersCritSec);
			Frame->ReadyTimestamp = RivermaxManager->GetTime();
			PendingFrames.RemoveSingle(Frame->FrameIndex);
			ReadyFrames.Add(Frame->FrameIndex);
		}

		OnFrameReadyDelegate.ExecuteIfBound();
	}

	void FFrameManager::MarkAsSending(const TSharedPtr<FRivermaxOutputFrame>& Frame)
	{
		// Make frame available to be sent
		FScopeLock Lock(&ContainersCritSec);
		if (ensure(!ReadyFrames.IsEmpty()))
		{
			SendingFrame = ReadyFrames[0];
			ensure(SendingFrame == Frame->FrameIndex);
			ReadyFrames.RemoveAt(0);
		}
	}

	const TSharedPtr<FRivermaxOutputFrame> FFrameManager::GetFrame(int32 Index) const
	{
		return Frames[Index];
	}

	bool FFrameManager::SetFrameData(const FRivermaxOutputVideoFrameInfo& NewFrameInfo)
	{
		bool bSuccess = false;
		if (TSharedPtr<FRivermaxOutputFrame> NextFrame = GetNextFrame(NewFrameInfo.FrameIdentifier))
		{
			TSharedPtr<FFrameBufferCopyInfo> Sidecar = MakeShared<FFrameBufferCopyInfo>();
			Sidecar->CopiedFrame = NextFrame;

			FCopyArgs Args;
			Args.RHISourceMemory = NewFrameInfo.GPUBuffer;
			Args.SourceMemory = NewFrameInfo.VideoBuffer;
			Args.DestinationMemory = NextFrame->VideoBuffer;
			Args.SizeToCopy = NewFrameInfo.Height * NewFrameInfo.Stride;
			Args.SideCar = MoveTemp(Sidecar);

			bSuccess = FrameAllocator->CopyData(Args);

			if (!bSuccess)
			{
				OnCriticalErrorDelegate.ExecuteIfBound();
			}
			 
		}
		return bSuccess;
	}

	void FFrameManager::OnDataCopied(const TSharedPtr<FBaseDataCopySideCar>& Payload)
	{
		TSharedPtr<FFrameBufferCopyInfo> CopyInfo = StaticCastSharedPtr<FFrameBufferCopyInfo>(Payload);
		if (ensure(CopyInfo && CopyInfo->CopiedFrame))
		{
			OnPreFrameReadyDelegate.ExecuteIfBound();

			if (TSharedPtr<FRivermaxOutputFrame> AvailableFrame = GetNextFrame(CopyInfo->CopiedFrame->FrameIdentifier))
			{
				// Video frame has been copied, update frame's state
				AvailableFrame->bIsVideoBufferReady = true;
				if (AvailableFrame->IsReadyToBeSent())
				{
					TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutFrameReadyTraceEvents[AvailableFrame->FrameIndex % 10]);
					TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[AvailableFrame->FrameIdentifier % 10]);

					MarkAsReady(AvailableFrame);
				}
			}
		}
		
	}
}

