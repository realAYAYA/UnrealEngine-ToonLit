// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Containers/SpscQueue.h"
#include "RivermaxFormats.h"
#include "RivermaxFrameAllocator.h"
#include "RivermaxOutputFrame.h"
#include "RivermaxTypes.h"

namespace UE::RivermaxCore
{
	class IRivermaxManager;
}

namespace UE::RivermaxCore::Private
{
	class FBaseFrameAllocator;
	struct FBaseDataCopySideCar;

	/** Where frame memory is allocated */
	enum class EFrameMemoryLocation : uint8
	{
		/** No memory was allocated */
		None,

		/** Memory allocated in system memory */
		System,

		/** Memory allocated on GPU. Cuda space is used at the moment. */
		GPU
	};

	DECLARE_DELEGATE(FOnFrameReadyDelegate);
	DECLARE_DELEGATE(FOnPreFrameReadyDelegate);
	DECLARE_DELEGATE(FOnFreeFrameDelegate);
	DECLARE_DELEGATE(FOnCriticalErrorDelegate);

	/** Holds arguments to configure frame manager during initialization */
	struct FFrameManagerSetupArgs
	{
		/** Resolution of video frames to allocate */
		FIntPoint Resolution = FIntPoint::ZeroValue;

		/** Stride of a line of video frame */
		uint32 Stride = 0;

		/** Desired size for a frame. Can be greater than what is needed to align with Rivermax's chunks */
		uint32 FrameDesiredSize = 0;

		/** Whether allocator will align each frame to desired alignment or the entire block */
		bool bAlignEachFrameAlloc = false;

		/** Number of video frames required */
		uint8 NumberOfFrames = 0;

		/** Whether we should try allocating on GPU */
		bool bTryGPUAllocation = true;

		/** Delegate called when a frame is now free to use */
		FOnFreeFrameDelegate OnFreeFrameDelegate;

		/** Delegate triggered just before a frame is enqueued to be sent */
		FOnPreFrameReadyDelegate OnPreFrameReadyDelegate;
		
		/** Delegate called when a frame is now ready to be sent */
		FOnFrameReadyDelegate OnFrameReadyDelegate;

		/** Delegate called when a critical has happened and stream should shut down */
		FOnCriticalErrorDelegate OnCriticalErrorDelegate;
	};

	/** 
	 * Class managing frames that we output over network 
	 * Handles memory allocation and state tracking
	 * 
	 * States of a frame
	 * 
	 * Free :	Frame can be used by the capture system.
	 * Pending:	Frame is being used by the capture system. 
	 *			Data isn't ready to be sent out yet but it's reserved for a given identifier.
	 * Ready:	Frame is ready to be sent. Data has been copied into it. 
	 * Sending: Frame is being actively sent out the wire. Can't modify it until next frame boundary
	 *
	 * Frame rate control
	 * 
	 * Sending a frame out takes a full frame interval so if capture system goes faster than output rate
	 * Free frames list will get depleted. If frame locking mode is used, getting the next free frame
	 * will block until a new one is available which will happen at next frame boundary
	 * Rendering and capturing the next frame might be quick but when ready to present it, it will get stalled.
	 * This will cause the engine's frame rate to match the output frame rate.
	 * 
	 */
	class FFrameManager
	{
	public:
		virtual ~FFrameManager();

		/** Initializes frame manager with a set of options. Returns where frames were allocated. */
		EFrameMemoryLocation Initialize(const FFrameManagerSetupArgs& Args);
		
		/** Requests cleanup of allocated memory */
		void Cleanup();

		/** Returns a frame not being used */
		TSharedPtr<FRivermaxOutputFrame> GetFreeFrame();

		/**
		 * Get next frame that can be used. 
		 * If a pending one matching identifier is found, it is returned.
		 * If not, a free frame will be returned, if any are available
		 */
		TSharedPtr<FRivermaxOutputFrame> GetNextFrame(uint32 NextFrameIdentifier);

		/** Returns a pending frame with the given identifier. If none is found, returns nullptr */
		TSharedPtr<FRivermaxOutputFrame> GetPendingFrame(uint32 FrameIdentifier);
		
		/** Returns next frame ready to be sent. Can be null. */
		TSharedPtr<FRivermaxOutputFrame> GetReadyFrame();

		/** Returns a frame for a given index */
		const TSharedPtr<FRivermaxOutputFrame> GetFrame(int32 Index) const;

		/** Mark a frame as being used and data should be coming soon */
		void MarkAsPending(const TSharedPtr<FRivermaxOutputFrame>& Frame);
		
		/** Mark a frame as being ready to be sent */
		void MarkAsReady(const TSharedPtr<FRivermaxOutputFrame>& Frame);
		
		/** Mark a frame as being sent */
		void MarkAsSending(const TSharedPtr<FRivermaxOutputFrame>& Frame);

		/** Mark a frame as sent */
		void MarkAsSent(const TSharedPtr<FRivermaxOutputFrame>& Frame);

		/** Initiates memory copy for a given frame */
		bool SetFrameData(const FRivermaxOutputVideoFrameInfo& NewFrame);

	private:

		/** Called back when copy request was completed by allocator */
		void OnDataCopied(const TSharedPtr<FBaseDataCopySideCar>& Sidecar);

	private:

		/** Resolution of video frames */
		FIntPoint FrameResolution = FIntPoint::ZeroValue;

		/** Number of frames allocated */
		uint32 TotalFrameCount = 0;

		/** Location of memory allocated */
		EFrameMemoryLocation MemoryLocation = EFrameMemoryLocation::None;

		/** Critical section to protect various frame containers */
		FCriticalSection ContainersCritSec;

		/** Frame allocator dealing with memory operation */
		TUniquePtr<FBaseFrameAllocator> FrameAllocator;

		/** List of allocated frames. */
		TArray<TSharedPtr<FRivermaxOutputFrame>> Frames;

		/** Delegate triggered when a frame is free to use */
		FOnFreeFrameDelegate OnFreeFrameDelegate;

		/** Delegate triggered just before a frame is enqueued to be sent */
		FOnPreFrameReadyDelegate OnPreFrameReadyDelegate;
		
		/** Delegate triggered when a frame is ready to be sent (video data has been copied) */
		FOnFrameReadyDelegate OnFrameReadyDelegate;

		/** Delegate triggered when a critical error has happened and stream should shut down */
		FOnCriticalErrorDelegate OnCriticalErrorDelegate;
		
		/** List of available frames. */
		TArray<int32> FreeFrames;
		
		/** List of frames being used but not ready to be sent yet  */
		TArray<int32> PendingFrames;

		/** List of frames ready to send */
		TArray<int32> ReadyFrames;

		/** Frame currently being sent */
		int32 SendingFrame = 0;

		/** Quick access to rivermax manager */
		TSharedPtr<IRivermaxManager> RivermaxManager;
	};
}


