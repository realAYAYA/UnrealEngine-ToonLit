// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/FrameRate.h"
#include "RivermaxFormats.h"

namespace UE::RivermaxCore
{
	enum class ERivermaxAlignmentMode
	{
		AlignmentPoint,
		FrameCreation,
	};

	struct RIVERMAXCORE_API FRivermaxInputStreamOptions
	{
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
		
		/** If true, don't use auto detected video format */
		bool bEnforceVideoFormat = false;

		/** Enforced resolution aligning with pgroup of sampling type */
		FIntPoint EnforcedResolution = FIntPoint::ZeroValue;

		/** Whether to leverage GPUDirect (Cuda) capability to transfer memory to NIC if available */
		bool bUseGPUDirect = true;
	};

	struct RIVERMAXCORE_API FRivermaxOutputStreamOptions
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
		bool bUseGPUDirect = true;

		/** Method used to align output stream */
		ERivermaxAlignmentMode AlignmentMode = ERivermaxAlignmentMode::AlignmentPoint;

		/** Whether the stream will output a frame at every frame interval, repeating last frame if no new one provided */
		bool bDoContinuousOutput = true;

		/** Whether to use frame's frame number instead of standard timestamping */
		bool bDoFrameCounterTimestamping = true;
	};

	enum class RIVERMAXCORE_API ERivermaxStreamType : uint8
	{
		VIDEO_2110_20_STREAM,
	};
}


