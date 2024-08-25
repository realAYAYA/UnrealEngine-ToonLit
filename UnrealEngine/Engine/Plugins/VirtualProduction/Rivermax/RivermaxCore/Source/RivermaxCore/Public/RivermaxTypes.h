// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/FrameRate.h"
#include "RivermaxFormats.h"

namespace UE::RivermaxCore
{
	constexpr TCHAR DefaultStreamAddress[] = TEXT("228.1.1.1");


	enum class ERivermaxAlignmentMode
	{
		/** Aligns scheduling with ST2059 frame boundary formula */
		AlignmentPoint,

		/** Aligns scheduling with frame creation */
		FrameCreation,
	};

	inline const TCHAR* LexToString(ERivermaxAlignmentMode InValue)
	{
		switch (InValue)
		{
			case ERivermaxAlignmentMode::AlignmentPoint:
			{
				return TEXT("Alignment point");
			}
			case ERivermaxAlignmentMode::FrameCreation:
			{
				return TEXT("Frame creation");
			}
			default:
			{
				checkNoEntry();
			}
		}

		return TEXT("<Unknown ERivermaxAlignmentMode>");
	}

	enum class EFrameLockingMode : uint8
	{
		/** If no frame available, continue */
		FreeRun,

		/** Blocks when reserving a frame slot. */
		BlockOnReservation,
	};

	inline const TCHAR* LexToString(EFrameLockingMode InValue)
	{
		switch (InValue)
		{
			case EFrameLockingMode::FreeRun:
			{
				return TEXT("Freerun");
			}
			case EFrameLockingMode::BlockOnReservation:
			{
				return TEXT("Blocking");
			}
			default:
			{
				checkNoEntry();
			}
		}

		return TEXT("<Unknown EFrameLockingMode>");
	}

	struct RIVERMAXCORE_API FRivermaxInputStreamOptions
	{
		/** Stream FrameRate */
		FFrameRate FrameRate = { 24,1 };

		/** Interface IP to bind to */
		FString InterfaceAddress;

		/** IP of the stream. Defaults to multicast group IP. */
		FString StreamAddress = DefaultStreamAddress;

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
		FString StreamAddress = DefaultStreamAddress;

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

		/** Defines how frame requests are handled. Whether they can block or not. */
		EFrameLockingMode FrameLockingMode = EFrameLockingMode::FreeRun;

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


