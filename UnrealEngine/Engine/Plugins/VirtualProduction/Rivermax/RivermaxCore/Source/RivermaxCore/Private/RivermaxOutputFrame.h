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
		~FRivermaxOutputFrame();
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

		/** Video buffer where we copy texture to be sent out */
		uint8* VideoBuffer = nullptr;

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

		/** Timestamp of this frame  */
		double TimestampTicks = 0.0;

		/** Pointers retrieved from Rivermax for the next chunk */
		void* PayloadPtr = nullptr;
		void* HeaderPtr = nullptr;

		
	};
}


