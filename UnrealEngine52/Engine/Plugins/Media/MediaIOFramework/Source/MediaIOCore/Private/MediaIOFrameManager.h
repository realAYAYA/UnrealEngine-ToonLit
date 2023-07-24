// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderingThread.h"

namespace UE::MediaCaptureData
{
	class FFrame
	{
	public:
		virtual ~FFrame() {}
		virtual int32 GetId() const = 0;
		virtual bool IsPending() const = 0;
	};

	/**
	 * Manages a queue of frames used by MediaCapture.
	 */
	class FFrameManager
	{
	public:
		~FFrameManager()
		{
			ENQUEUE_RENDER_COMMAND(MediaCaptureFrameManagerCleaning)(
				[FramesToBeReleased = MoveTemp(Frames)](FRHICommandListImmediate& RHICmdList) mutable
			{
				FramesToBeReleased.Reset();
			});
		}

		void ForEachFrame(TFunctionRef<void(const TSharedPtr<FFrame>&)> Function)
		{
			for (const TSharedPtr<FFrame>& Frame : Frames)
			{
				Function(Frame);
			}
		}

		FString GetFramesState()
		{
			TStringBuilder<256> ReadbackInfoBuilder;
			ReadbackInfoBuilder << "\n";
			for (int32 Index = 0; Index < Frames.Num(); Index++)
			{
				ReadbackInfoBuilder << FString::Format(TEXT("Frame {0} State:  {1}\n"), { Index, Frames[Index]->IsPending() ? TEXT("Pending") : TEXT("Idle") });
			}

			return ReadbackInfoBuilder.ToString();
		}

		TConstArrayView<TSharedPtr<FFrame>> GetFrames() const
		{
			return Frames;
		}

		void AddFrame(TSharedPtr<FFrame> NewFrame)
		{
			AvailableFrames.Enqueue(NewFrame->GetId());
			Frames.Add(MoveTemp(NewFrame));
		}

		void MarkAvailable(const FFrame& InFrame)
		{
			if (ensure(!InFrame.IsPending()))
			{
				AvailableFrames.Enqueue(InFrame.GetId());
			}
		}

		void MarkPending(const FFrame& InFrame)
		{
			if (ensure(InFrame.IsPending()))
			{
				PendingFrames.Enqueue(InFrame.GetId());
			}
		}

		void CompleteNextPending(const FFrame& InFrame)
		{
			TOptional<int32> NextPending = PendingFrames.Dequeue();
			if (ensure(NextPending.IsSet() && InFrame.GetId() == NextPending.GetValue()))
			{
				MarkAvailable(InFrame);
			}
		}

		template<typename FrameType>
		TSharedPtr<FrameType> GetNextAvailable()
		{
			TSharedPtr<FrameType> NextAvailableFrame;
			
			static_assert(TIsDerivedFrom<FrameType, FFrame>::Value, "The frame type must derive from FFrame.");
			TOptional<int32> NextAvailable = AvailableFrames.Dequeue();
			if (NextAvailable.IsSet())
			{
				NextAvailableFrame = StaticCastSharedPtr<FrameType>(Frames[NextAvailable.GetValue()]);
			}

			// Peek next pending before capturing to avoid getting the newly captured one. 
			// It shouldn't happen but if it happens it will start waiting for the capture to complete
			TSharedPtr<FFrame> NextPendingFrame = PeekNextPending<FFrame>();
			if (NextPendingFrame && NextAvailableFrame)
			{
				ensureMsgf(NextPendingFrame->GetId() != NextAvailableFrame->GetId(), TEXT("The next pending frame was the same as the next available frame, this should not have happened!"));
			}

			return NextAvailableFrame;
		}

		template<typename FrameType>
		TSharedPtr<FrameType> PeekNextPending()
		{
			static_assert(TIsDerivedFrom<FrameType, FFrame>::Value, "The frame type must derive from FFrame.");
			if (const int32* NextPending = PendingFrames.Peek())
			{
				return StaticCastSharedPtr<FrameType>(Frames[*NextPending]);
			}

			return nullptr;
		}

	private:
		TArray<TSharedPtr<FFrame>> Frames;
		TSpscQueue<int32> AvailableFrames;
		TSpscQueue<int32> PendingFrames;
	};
}
