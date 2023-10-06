// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Math/NumericLimits.h"
#include "Misc/Timecode.h"

namespace UE::RivermaxCore::Private
{
	/**
	 * Output frame descriptor. Contains data to be sent on the wire and packetization tracking
	 */
	struct FRivermaxOutputFrame
	{
		FRivermaxOutputFrame(uint32 InFrameIndex);
		FRivermaxOutputFrame(const FRivermaxOutputFrame&) = delete;
		FRivermaxOutputFrame& operator=(const FRivermaxOutputFrame&) = delete;

		/** Whether this frame is ready to be sent */
		bool IsReadyToBeSent() const;

		/** Reset internal counters to make it resendable */
		void Clear();

		/** Reset identifier and internal counters to make it available */
		void Reset();

		/** Index of the frame used as unique identifier */
		uint32 FrameIndex = 0;

		/** 
		 * Video buffer where we copy texture to be sent out 
		 * If using GPUDirect, memory will be allocated in cuda space
		 */
		void* VideoBuffer = nullptr;

		/** Identifier of the frame incremented every time a frame is captured */
		static constexpr uint32 InvalidIdentifier = MAX_uint32;
		uint32 FrameIdentifier = InvalidIdentifier;

		/** Timecode at which this frame was captured */
		FTimecode Timecode;

		/** Used to flag this frame ready to be sent */
		bool bIsVideoBufferReady = false;

		/** Variables about progress of packetization of the frame */
		uint32 PacketCounter = 0;
		uint32 LineNumber = 0;
		uint16 SRDOffset = 0;
		uint32 ChunkNumber = 0;
		uint32 BytesSent = 0;

		/** Timestamp of this frame used for RTP headers  */
		uint32 MediaTimestamp = 0;

		/** Payload (data) pointer retrieved from Rivermax for the next chunk */
		void* PayloadPtr = nullptr;
		
		/** Header pointer retrieved from Rivermax for the next chunk */
		void* HeaderPtr = nullptr;
		
		/** Cached address of beginning of frame in Rivermax's memblock. Used when using intermediate buffer/ */
		void* FrameStartPtr = nullptr;
		
		/** Offset in the frame where we are at to copy next block of data */
		uint32 Offset = 0;
		
		/** Time covered by memcopies. Not used for now but it could be used to detect if memory will be used before it's actually copied. */
		uint64 ScheduleTimeCopied = 0;

		/** Time at which this frame was made available to be sent */
		uint64 ReadyTimestamp = 0;

		/** Whether timing issues were detected while sending frame out. If yes, we skip the next frame boundary */
		bool bCaughtTimingIssue = false;
	};
}


