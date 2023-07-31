// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/FrameRate.h"

namespace UE::RivermaxCore
{
	enum class RIVERMAXCORE_API ESamplingType : uint8
	{
		YUV422_8bit
		, YUV422_10bit
		, YUV422_12bit
		, YUV422_16bit
		, YUV422_16bitFloat
		, YUV444_8bit
		, YUV444_10bit
		, YUV444_12bit
		, YUV444_16bit
		, YUV444_16bitFloat
		, RGB_8bit
		, RGB_10bit
		, RGB_12bit
		, RGB_16bit
		, RGB_16bitFloat
	};

	struct RIVERMAXCORE_API FVideoFormatInfo
	{
		/** Sampling type of this format */
		ESamplingType Sampling = ESamplingType::RGB_10bit;

		/** Bit depth for this format (i.e 8,10,12,16) */
		uint16 BitDepth = 0;

		/** Pixel Group (pgroup) size in bytes */
		uint16 PixelGroupSize = 0;

		/** Pixel Group coverage (pgroup coverage) in pixels */
		uint16 PixelGroupCoverage = 0;
	};

	struct RIVERMAXCORE_API FStandardVideoFormat
	{
		static FVideoFormatInfo GetVideoFormatInfo(ESamplingType SamplingType);
		static bool IsRGB(ESamplingType SamplingType);
		static bool IsYUV(ESamplingType SamplingType);
		static bool IsYUV422(ESamplingType SamplingType);
		static bool IsYUV444(ESamplingType SamplingType);

	private:
		static const TArray<FVideoFormatInfo> AllSamplingTypes;
	};
}


