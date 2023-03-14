// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/FrameRate.h"
#include "RivermaxFormats.h"

namespace UE::RivermaxCore
{
	struct RIVERMAXCORE_API FRivermaxStreamOptions
	{
		/** Desired stream resolution */
		FIntPoint Resolution = { 1920, 1080 };

		/** Stream FrameRate */
		FFrameRate FrameRate = { 24,1 };

		/** Interface IP to bind to */
		FString InterfaceAddress;

		/** IP of the stream. Defaults to multicast group IP. */
		FString StreamAddress = TEXT("224.1.1.1");

		/** Port to be used by stream */
		uint32 Port = 50000;

		/** Desired stream pixel format */
		ESamplingType PixelFormat = ESamplingType::RGB_10bit;

		/** Sample count to buffer. */
		int32 NumberOfBuffers = 2;

		/** Resolution aligning with pgroup of sampling type */
		FIntPoint AlignedResolution = FIntPoint::ZeroValue;

		/** Whether to leverage GPUDirect (Cuda) capability to transfer memory to NIC if available */
		bool bUseGPUDirect = false;
	};

	enum class RIVERMAXCORE_API ERivermaxStreamType : uint8
	{
		VIDEO_2110_20_STREAM,

		/** Todo add additional stream types */
		//VIDEO_2110_22_STREAM, compressed video
		//AUDIO_2110_30_31_STREAM,
		//ANCILLARY_2110_40_STREAM,
	};
}


