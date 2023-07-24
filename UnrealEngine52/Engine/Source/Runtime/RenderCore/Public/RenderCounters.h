// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreGlobals.h"



class FPixelRenderCounters
{
public:
	uint32 GetPixelRenderCount() const
	{
		return PrevPixelRenderCount;
	}

	uint32 GetPixelDisplayCount() const
	{
		return PrevPixelDisplayCount;
	}

	void AddViewStatistics(uint32 PixelRenderCount, uint32 PixelDisplayCount)
	{
		check(IsInRenderingThread());
		CurrentPixelRenderCount += PixelRenderCount;
		CurrentPixelDisplayCount += PixelDisplayCount;
	}

private:
	uint32 PrevPixelRenderCount = 0;
	uint32 PrevPixelDisplayCount = 0;
	uint32 CurrentPixelRenderCount = 0;
	uint32 CurrentPixelDisplayCount = 0;

	friend void TickPixelRenderCounters();
};

extern RENDERCORE_API FPixelRenderCounters GPixelRenderCounters;
