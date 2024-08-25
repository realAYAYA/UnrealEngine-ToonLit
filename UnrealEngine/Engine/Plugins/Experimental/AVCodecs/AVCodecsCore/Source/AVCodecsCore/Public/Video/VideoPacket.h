// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVPacket.h"
#include "Templates/SharedPointerFwd.h"

/**
 * Video-specific packet container.
 */
struct FVideoPacket : public FAVPacket
{
public:
	/**
	 * QP that the raw data was encoded with.
	 */
	uint32 QP;

	/**
	 * Whether this frame is a keyframe.
	 */
	uint8 bIsKeyframe : 1;

	FVideoPacket() = default;
	FVideoPacket(TSharedPtr<uint8> const& DataPtr, uint64 DataSize, uint64 Timestamp, uint64 Index, uint32 QP, bool bIsKeyframe)
		: FAVPacket(DataPtr, DataSize, Timestamp, Index)
		, QP(QP)
		, bIsKeyframe(bIsKeyframe)
	{
	}
};
