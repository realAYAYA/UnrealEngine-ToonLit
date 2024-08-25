// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPixelCaptureFrameMetadata
{
	// Identifier for the capture pipeline/process this frame took
	FString ProcessName = "Unknown";

	// Identifier for the frame
	uint64 Id = 0;

	// Which layer this specific frame is associated with
	int32 Layer = 0;

	// The time this frame was sourced/created
	uint64 SourceTime = 0;

	// Capture process timings. Duration not timestamp.
	uint64 CaptureTime = 0;
	uint64 CaptureProcessCPUTime = 0;
	uint64 CaptureProcessGPUDelay = 0;
	uint64 CaptureProcessGPUTime = 0;

	// Display process timings. Duration not timestamp.
	uint64 DisplayTime = 0;

	// Frame use timings (can happen multiple times. ie. we are consuming frames faster than producing them)
	uint32 UseCount = 0; // how many times the frame has been fed to the encoder or decoder

	// Encode Timings
	uint64 FirstEncodeStartTime = 0;
	uint64 LastEncodeStartTime = 0;
	uint64 LastEncodeEndTime = 0;

	// Packet Timings
	uint64 FirstPacketizationStartTime = 0;
	uint64 LastPacketizationStartTime = 0;
	uint64 LastPacketizationEndTime = 0;

	// Decode Timings
	uint64 FirstDecodeStartTime = 0;
	uint64 LastDecodeStartTime = 0;
	uint64 LastDecodeEndTime = 0;

	// wanted this to be explicit with a name
	FPixelCaptureFrameMetadata Copy() const { return *this; }
};
