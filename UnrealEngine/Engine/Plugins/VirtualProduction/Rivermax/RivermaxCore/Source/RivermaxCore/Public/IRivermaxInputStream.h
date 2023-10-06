// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RivermaxFormats.h"

class FRHIBuffer;

namespace UE::RivermaxCore
{
	struct FRivermaxInputStreamOptions;

	struct RIVERMAXCORE_API FRivermaxInputInitializationResult
	{
		/** Whether initialization suceeded */
		bool bHasSucceed = false;

		/** Whether gpudirect can be used, if requested in the first place */
		bool bIsGPUDirectSupported = false;
	};

	struct RIVERMAXCORE_API FRivermaxInputVideoFrameDescriptor
	{
		/** Height of the received frame */
		uint32 Height = 0;

		/** Width of the received frame */
		uint32 Width = 0;

		/** Total size of the video frame */
		uint32 VideoBufferSize = 0;

		/** Timestamp, in media clock realm, marked by the sender */
		uint32 Timestamp = 0;

		/** Frame number derived from timestamp and frame rate */
		uint32 FrameNumber = 0;

		/** Pixel format of the frame */
		ESamplingType PixelFormat = ESamplingType::RGB_10bit;
	};

	struct RIVERMAXCORE_API FRivermaxInputVideoFormatChangedInfo
	{
		/** Detected height in pixel of the video stream */
		uint32 Height = 0;

		/** Detected width in pixel of the video stream */
		uint32 Width = 0;

		/** Detected sampling type of the video stream */
		ESamplingType PixelFormat = ESamplingType::RGB_10bit;
	};

	struct RIVERMAXCORE_API FRivermaxInputVideoFrameRequest
	{
		/** Buffer pointer in RAM where to write incoming frame */
		uint8* VideoBuffer = nullptr;
		
		/** Buffer pointer in GPU to be mapped to cuda and where to write incoming frame */
		FRHIBuffer* GPUBuffer = nullptr;
	};

	struct RIVERMAXCORE_API FRivermaxInputVideoFrameReception
	{
		uint8* VideoBuffer = nullptr;
	};

	class RIVERMAXCORE_API IRivermaxInputStreamListener
	{
	public:
		/** Initialization completion callback with result */
		virtual void OnInitializationCompleted(const FRivermaxInputInitializationResult& Result) = 0;
	
		/** Called when stream is ready to fill the next frame. Returns true if a frame was successfully requested */
		virtual bool OnVideoFrameRequested(const FRivermaxInputVideoFrameDescriptor& FrameInfo, FRivermaxInputVideoFrameRequest& OutVideoFrameRequest) = 0;
		
		/** Called when a frame has been received */
		virtual void OnVideoFrameReceived(const FRivermaxInputVideoFrameDescriptor& FrameInfo, const FRivermaxInputVideoFrameReception& ReceivedVideoFrame) = 0;

		/** Called when an error was encountered during frame reception */
		virtual void OnVideoFrameReceptionError(const FRivermaxInputVideoFrameDescriptor& FrameInfo) {};

		/** Called when stream has encountered an error and has to stop */
		virtual void OnStreamError() = 0;

		/**  Called when stream has detected a change in the video format */
		virtual void OnVideoFormatChanged(const FRivermaxInputVideoFormatChangedInfo& NewFormatInfo) = 0;
	};

	class RIVERMAXCORE_API IRivermaxInputStream
	{
	public:
		virtual ~IRivermaxInputStream() = default;

	public:
		virtual bool Initialize(const FRivermaxInputStreamOptions& InOptions, IRivermaxInputStreamListener& InListener) = 0;
		virtual void Uninitialize() = 0;
	};
}

