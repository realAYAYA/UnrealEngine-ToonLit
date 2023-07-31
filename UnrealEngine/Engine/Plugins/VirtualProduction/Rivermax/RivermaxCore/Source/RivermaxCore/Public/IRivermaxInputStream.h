// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::RivermaxCore
{
	struct FRivermaxStreamOptions;

	struct RIVERMAXCORE_API FRivermaxInputVideoFrameDescriptor
	{
		uint32 Height = 0;
		uint32 Width = 0;
		uint32 Stride = 0;
		uint32 VideoBufferSize = 0;
	};

	struct RIVERMAXCORE_API FRivermaxInputVideoFrameRequest
	{
		uint8* VideoBuffer = nullptr;
	};

	struct RIVERMAXCORE_API FRivermaxInputVideoFrameReception
	{
		uint8* VideoBuffer = nullptr;
	};

	class RIVERMAXCORE_API IRivermaxInputStreamListener
	{
	public:
		/** Initialization completion callback with result */
		virtual void OnInitializationCompleted(bool bHasSucceed) = 0;
	
		/** Called when stream is ready to fill the next frame. Returns true if a frame was successfully requested */
		virtual bool OnVideoFrameRequested(const FRivermaxInputVideoFrameDescriptor& FrameInfo, FRivermaxInputVideoFrameRequest& OutVideoFrameRequest) = 0;
		
		/** Called when a frame has been received */
		virtual void OnVideoFrameReceived(const FRivermaxInputVideoFrameDescriptor& FrameInfo, const FRivermaxInputVideoFrameReception& ReceivedVideoFrame) = 0;
	};

	class RIVERMAXCORE_API IRivermaxInputStream
	{
	public:
		virtual ~IRivermaxInputStream() = default;

	public:
		virtual bool Initialize(const FRivermaxStreamOptions& InOptions, IRivermaxInputStreamListener& InListener) = 0;
		virtual void Uninitialize() = 0;
	};
}

