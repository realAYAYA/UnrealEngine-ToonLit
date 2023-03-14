// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RHI.h"


namespace UE::RivermaxCore
{
	struct FRivermaxStreamOptions;

	struct RIVERMAXCORE_API FRivermaxOutputVideoFrameInfo
	{
		uint32 FrameIdentifier = 0;
		uint32 Height = 0;
		uint32 Width = 0;
		uint32 Stride = 0;
		void* VideoBuffer = nullptr;
	};

	class RIVERMAXCORE_API IRivermaxOutputStreamListener
	{
	public:

		/** Initialization completion callback with result */
		virtual void OnInitializationCompleted(bool bHasSucceed) = 0;

		/** Called when stream has encountered an error and has to stop */
		virtual void OnStreamError() = 0;
	};

	class RIVERMAXCORE_API IRivermaxOutputStream
	{
	public:
		virtual ~IRivermaxOutputStream() = default;

	public:

		/** 
		 * Initializes stream using input options. Returns false if stream creation has failed. 
		 * Initialization is completed when OnInitializationCompletion has been called with result.
		 */
		virtual bool Initialize(const FRivermaxStreamOptions& Options, IRivermaxOutputStreamListener& InListener) = 0;

		/** Uninitializes current stream */
		virtual void Uninitialize() = 0;

		/** Pushes new video frame to the stream. Returns false if frame couldn't be pushed. */
		virtual bool PushVideoFrame(const FRivermaxOutputVideoFrameInfo& NewFrame) = 0;
		
		/** Pushes a frame to be captured from GPU memory */
		virtual bool PushGPUVideoFrame(const FRivermaxOutputVideoFrameInfo& NewFrame, FBufferRHIRef CapturedBuffer) = 0;

		/** Returns true if GPUDirect is supported */
		virtual bool IsGPUDirectSupported() const = 0;
	};
}


