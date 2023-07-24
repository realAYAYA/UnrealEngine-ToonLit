// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace BlackmagicDesign
{
	namespace Private
	{
		bool IsSSE2Available();
		bool IsCorrectlyAlignedForSSE2MemCpy(const void* InDst, const void* InSrc, unsigned int InSize);
		void SSE2MemCpy(const void* InDst, const void* InSrc, unsigned int InSize);
	}
}
