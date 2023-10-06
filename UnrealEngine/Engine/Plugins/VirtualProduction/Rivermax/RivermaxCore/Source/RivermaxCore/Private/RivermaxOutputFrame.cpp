// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxOutputFrame.h"

namespace UE::RivermaxCore::Private
{
	FRivermaxOutputFrame::FRivermaxOutputFrame(uint32 InFrameIndex)
		: FrameIndex(InFrameIndex)
		, VideoBuffer(nullptr)
	{
		Clear();
	}

	bool FRivermaxOutputFrame::IsReadyToBeSent() const
	{
		return FrameIdentifier != InvalidIdentifier && bIsVideoBufferReady;
	}

	void FRivermaxOutputFrame::Clear()
	{
		PacketCounter = 0;
		LineNumber = 0;
		SRDOffset = 0;
		BytesSent = 0;
	}

	void FRivermaxOutputFrame::Reset()
	{
		FrameIdentifier = InvalidIdentifier;
		bIsVideoBufferReady = false;
		bCaughtTimingIssue = false;
		ReadyTimestamp = 0;
		Clear();
	}

}
