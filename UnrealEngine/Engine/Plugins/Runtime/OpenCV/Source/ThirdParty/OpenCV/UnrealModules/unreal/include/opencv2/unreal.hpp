// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <opencv2/core.hpp>

namespace cv 
{
	namespace unreal
	{
		/** Signature of malloc that Unreal may replace using SetMallocAndFree */
		typedef void *(*TUnrealMalloc)(size_t, uint32_t);

		/** Signature of free that Unreal may replace using SetMallocAndFree */
		typedef void (*TUnrealFree)(void*);

		/** Replaces the malloc and free functions used in the operator new/delete overrides */
		CV_EXPORTS_W void SetMallocAndFree(TUnrealMalloc, TUnrealFree);
	}
}
