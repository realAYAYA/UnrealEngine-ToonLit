// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaUtils.h"

#include "RivermaxShaders.h"

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

	FSourceBufferDesc GetBufferDescription(const FIntPoint& InResolution, ERivermaxMediaSourcePixelFormat InPixelFormat)
	{
		using namespace UE::RivermaxCore;
		using namespace UE::RivermaxShaders;

		FSourceBufferDesc Description;

		const ESamplingType SamplingType = MediaSourcePixelFormatToRivermaxSamplingType(InPixelFormat);
		const FVideoFormatInfo Info = FStandardVideoFormat::GetVideoFormatInfo(SamplingType);

		// Compute horizontal byte count (stride) of resolution
		const uint32 PixelAlignment = Info.PixelGroupCoverage;
		const uint32 PixelCount = InResolution.X * InResolution.Y;
		ensureMsgf(PixelCount % PixelAlignment == 0, TEXT("Unaligned resolution (%dx%d) provided for pixel format %s"), InResolution.X, InResolution.Y, *UEnum::GetValueAsString(InPixelFormat));
		const uint32 PixelGroupCount = PixelCount / PixelAlignment;
		const uint32 FrameByteCount = PixelGroupCount * Info.PixelGroupSize;

		switch (SamplingType)
		{
		case ESamplingType::YUV422_8bit:
		{
			Description.BytesPerElement = sizeof(FRGBToYUV8Bit422CS::FYUV8Bit422Buffer);
			break;
		}
		case ESamplingType::YUV422_10bit:
		{
			Description.BytesPerElement = sizeof(FRGBToYUV10Bit422LittleEndianCS::FYUV10Bit422LEBuffer);
			break;
		}
		case ESamplingType::RGB_8bit:
		{
			Description.BytesPerElement = sizeof(FRGBToRGB8BitCS::FRGB8BitBuffer);
			break;
		}
		case ESamplingType::RGB_10bit:
		{
			Description.BytesPerElement = sizeof(FRGB10BitToRGBA10CS::FRGB10BitBuffer);
			break;
		}
		case ESamplingType::RGB_12bit:
		{
			Description.BytesPerElement = sizeof(FRGBToRGB12BitCS::FRGB12BitBuffer);
			break;
		}
		case ESamplingType::RGB_16bitFloat:
		{
			Description.BytesPerElement = sizeof(FRGBToRGB16fCS::FRGB16fBuffer);
			break;
		}
		default:
		{
			checkNoEntry(); 
			return Description;
		}
		}

		// Shader encoding might not align with pixel group size so we need to have enough elements to represent the last pixel group
		Description.NumberOfElements = FrameByteCount / Description.BytesPerElement;
		Description.NumberOfElements += FrameByteCount % Description.BytesPerElement != 0 ? 1 : 0;
		
		return Description;
	}

	UE::RivermaxCore::ERivermaxAlignmentMode MediaOutputAlignmentToRivermaxAlignment(ERivermaxMediaAlignmentMode InAlignmentMode)
	{
		using namespace UE::RivermaxCore;

		switch (InAlignmentMode)
		{
		case ERivermaxMediaAlignmentMode::AlignmentPoint:
		{
			return ERivermaxAlignmentMode::AlignmentPoint;
		}
		case ERivermaxMediaAlignmentMode::FrameCreation:
		{
			return ERivermaxAlignmentMode::FrameCreation;
		}
		default:
		{
			checkNoEntry();
			return ERivermaxAlignmentMode::AlignmentPoint;
		}
		}
	}

	ERivermaxMediaSourcePixelFormat RivermaxPixelFormatToMediaSourcePixelFormat(UE::RivermaxCore::ESamplingType InSamplingType)
	{
		using namespace UE::RivermaxCore;

		switch (InSamplingType)
		{
		case ESamplingType::YUV422_8bit:
		{
			return ERivermaxMediaSourcePixelFormat::YUV422_8bit;
		}
		case ESamplingType::YUV422_10bit:
		{
			return ERivermaxMediaSourcePixelFormat::YUV422_10bit;
		}
		case ESamplingType::RGB_8bit:
		{
			return ERivermaxMediaSourcePixelFormat::RGB_8bit;
		}
		case ESamplingType::RGB_10bit:
		{
			return ERivermaxMediaSourcePixelFormat::RGB_10bit;
		}
		case ESamplingType::RGB_12bit:
		{
			return ERivermaxMediaSourcePixelFormat::RGB_12bit;
		}
		case ESamplingType::RGB_16bitFloat:
		{
			return ERivermaxMediaSourcePixelFormat::RGB_16bit_Float;
		}
		default:
		{
			checkNoEntry();
			return ERivermaxMediaSourcePixelFormat::RGB_10bit;
		}
		}
	}

	ERivermaxMediaOutputPixelFormat RivermaxPixelFormatToMediaOutputPixelFormat(UE::RivermaxCore::ESamplingType InSamplingType)
	{
		using namespace UE::RivermaxCore;

		switch (InSamplingType)
		{
		case ESamplingType::YUV422_8bit:
		{
			return ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422;
		}
		case ESamplingType::YUV422_10bit:
		{
			return ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422;
		}
		case ESamplingType::RGB_8bit:
		{
			return ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB;
		}
		case ESamplingType::RGB_10bit:
		{
			return ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB;
		}
		case ESamplingType::RGB_12bit:
		{
			return ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB;
		}
		case ESamplingType::RGB_16bitFloat:
		{
			return ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB;
		}
		default:
		{
			checkNoEntry();
			return ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB;
		}
		}
	}

	UE::RivermaxCore::EFrameLockingMode MediaOutputFrameLockingToRivermax(ERivermaxFrameLockingMode InFrameLockingMode)
	{
		using namespace UE::RivermaxCore;

		switch (InFrameLockingMode)
		{
		case ERivermaxFrameLockingMode::FreeRun:
		{
			return EFrameLockingMode::FreeRun;
		}
		case ERivermaxFrameLockingMode::BlockOnReservation:
		{
			return EFrameLockingMode::BlockOnReservation;
		}
		default:
		{
			checkNoEntry();
			return EFrameLockingMode::FreeRun;
		}
		}
	}

}
