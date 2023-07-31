// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WmfPrivate.h"

#include "HAL/ThreadSafeBool.h"
#include "MediaPacket.h"

class FWmfRingBuffer
{
public:
	FTimespan GetMaxDuration() const
	{
		return MaxDuration;
	}

	void SetMaxDuration(FTimespan InMaxDuration)
	{
		MaxDuration = InMaxDuration;
	}

	FTimespan GetDuration() const;

	void Push(AVEncoder::FMediaPacket&& Sample);

	void PauseCleanup(bool bPause);

	TArray<AVEncoder::FMediaPacket> GetCopy();

	void Reset();

private:
	void Cleanup();

private:
	FTimespan MaxDuration = 0;
	TArray<AVEncoder::FMediaPacket> Samples;
	FCriticalSection Mutex;
	FThreadSafeBool bCleanupPaused = false;
};

