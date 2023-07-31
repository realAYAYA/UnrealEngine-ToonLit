// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaUtils.h"

namespace UE::RivermaxMediaUtils::Private
{
	UE::RivermaxCore::ESamplingType MediaOutputPixelFormatToRivermaxSamplingType(ERivermaxMediaOutputPixelFormat InPixelFormat)
	{
		using namespace UE::RivermaxCore;

		switch (InPixelFormat)
		{
		case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
		{
			return ESamplingType::YUV422_8bit;
		}
		case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
		{
			return ESamplingType::YUV422_10bit;
		}
		case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
		{
			return ESamplingType::RGB_8bit;
		}
		case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
		{
			return ESamplingType::RGB_10bit;
		}
		case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
		{
			return ESamplingType::RGB_12bit;
		}
		case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
		{
			return ESamplingType::RGB_16bitFloat;
		}
		default:
		{
			checkNoEntry();
			return ESamplingType::RGB_10bit;
		}
		}
	}

	UE::RivermaxCore::ESamplingType MediaSourcePixelFormatToRivermaxSamplingType(ERivermaxMediaSourcePixelFormat InPixelFormat)
	{
		using namespace UE::RivermaxCore;

		switch (InPixelFormat)
		{
		case ERivermaxMediaSourcePixelFormat::YUV422_8bit:
		{
			return ESamplingType::YUV422_8bit;
		}
		case ERivermaxMediaSourcePixelFormat::YUV422_10bit:
		{
			return ESamplingType::YUV422_10bit;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_8bit:
		{
			return ESamplingType::RGB_8bit;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_10bit:
		{
			return ESamplingType::RGB_10bit;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_12bit:
		{
			return ESamplingType::RGB_12bit;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_16bit_Float:
		{
			return ESamplingType::RGB_16bitFloat;
		}
		default:
		{
			checkNoEntry();
			return ESamplingType::RGB_10bit;
		}
		}
	}
}
