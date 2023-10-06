// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if !UE_SERVER

#include "ElectraTextureSample.h"

// -------------------------------------------------------------------------------------------------------------------------------------------------------

void IElectraTextureSampleBase::Initialize(FVideoDecoderOutput* InVideoDecoderOutput)
{
	VideoDecoderOutput = StaticCastSharedPtr<FVideoDecoderOutput, IDecoderOutputPoolable, ESPMode::ThreadSafe>(InVideoDecoderOutput->AsShared());
	HDRInfo = VideoDecoderOutput->GetHDRInformation();
	Colorimetry = VideoDecoderOutput->GetColorimetry();

	bool bFullRange = false;
	if (auto PinnedColorimetry = Colorimetry.Pin())
	{
		bFullRange = (PinnedColorimetry->GetMPEGDefinition()->VideoFullRangeFlag != 0);
	}
	
	EPixelFormat PixFmt = VideoDecoderOutput->GetFormat();
	uint8 NumBits = 8;
	if (!IsDXTCBlockCompressedTextureFormat(PixFmt))
	{
		if (PixFmt == PF_NV12)
		{
			NumBits = 8;
		}
		else if (PixFmt == PF_A2B10G10R10)
		{
			NumBits = 10;
		}
		else if (PixFmt == PF_P010)
		{
			NumBits = 16;
		}
		else
		{
			NumBits = (8 * GPixelFormats[PixFmt].BlockBytes) / GPixelFormats[PixFmt].NumComponents;
		}
	}

	// Prepare YUV -> RGB matrix containing all necessary offsets and scales to produce RGB straight from sample data
	const FMatrix* Mtx = bFullRange ? &MediaShaders::YuvToRgbRec709Unscaled : &MediaShaders::YuvToRgbRec709Scaled;
	FVector Off;
	switch (NumBits)
	{
		case 8:		Off = bFullRange ? MediaShaders::YUVOffsetNoScale8bits  : MediaShaders::YUVOffset8bits; break;
		case 10:	Off = bFullRange ? MediaShaders::YUVOffsetNoScale10bits : MediaShaders::YUVOffset10bits; break;
		case 32:	Off = bFullRange ? MediaShaders::YUVOffsetNoScaleFloat  : MediaShaders::YUVOffsetFloat; break;
		default:	Off = bFullRange ? MediaShaders::YUVOffsetNoScale16bits : MediaShaders::YUVOffset16bits; break;
	}
	// Correctional scale for input data
	// (data should be placed in the upper 10-bits of the 16-bit texture channels, but some platforms do not do this)
	float DataScale = GetSampleDataScale(false);

	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		switch (PinnedHDRInfo->GetHDRType())
		{
		case IVideoDecoderHDRInformation::EType::PQ10:
		case IVideoDecoderHDRInformation::EType::HLG10:
		case IVideoDecoderHDRInformation::EType::HDR10:
			Mtx = &MediaShaders::YuvToRgbRec2020Unscaled;
			Off = MediaShaders::YUVOffsetNoScale16bits;
			DataScale = GetSampleDataScale(true);
			break;
		default:
			break;
		}
	}

	// Compute scale to make correct towards the max value (1.0 if it is 0xff or 0xffff, but >1.0 if it 0xffc0 - which is the case for P010)
	float NormScale = (VideoDecoderOutput->GetFormat() == PF_P010) ? (65535.0f / 65472.0f) : 1.0f;

	// Matrix to transform sample data to standard YUV values
	FMatrix PreMtx = FMatrix::Identity;
	PreMtx.M[0][0] = DataScale * NormScale;
	PreMtx.M[1][1] = DataScale * NormScale;
	PreMtx.M[2][2] = DataScale * NormScale;
	PreMtx.M[0][3] = -Off.X * NormScale;
	PreMtx.M[1][3] = -Off.Y * NormScale;
	PreMtx.M[2][3] = -Off.Z * NormScale;

	// Combine this with the actual YUV-RGB conversion
	YuvToRgbMtx = FMatrix44f(*Mtx * PreMtx);
}

void IElectraTextureSampleBase::InitializePoolable()
{
}

void IElectraTextureSampleBase::ShutdownPoolable()
{
	VideoDecoderOutput.Reset();
}

FIntPoint IElectraTextureSampleBase::GetDim() const
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutput->GetDim();
	}
	return FIntPoint::ZeroValue;
}


FIntPoint IElectraTextureSampleBase::GetOutputDim() const
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutput->GetOutputDim();
	}
	return FIntPoint::ZeroValue;
}


FMediaTimeStamp IElectraTextureSampleBase::GetTime() const
{
	if (VideoDecoderOutput)
	{
		const FDecoderTimeStamp TimeStamp = VideoDecoderOutput->GetTime();
		return FMediaTimeStamp(TimeStamp.Time, TimeStamp.SequenceIndex);
	}
	return FMediaTimeStamp();
}


FTimespan IElectraTextureSampleBase::GetDuration() const
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutput->GetDuration();
	}
	return FTimespan::Zero();
}

bool IElectraTextureSampleBase::IsOutputSrgb() const
{
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		// If the HDR type is unknown we also assume sRGB
		return (PinnedHDRInfo->GetHDRType() == IVideoDecoderHDRInformation::EType::Unknown);
	}
	// If no HDR info is present, we assume sRGB
	return true;
}

const FMatrix& IElectraTextureSampleBase::GetYUVToRGBMatrix() const
{
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		switch (PinnedHDRInfo->GetHDRType())
		{
		case IVideoDecoderHDRInformation::EType::PQ10:
		case IVideoDecoderHDRInformation::EType::HLG10:
		case IVideoDecoderHDRInformation::EType::HDR10:		return MediaShaders::YuvToRgbRec2020Unscaled;
		case IVideoDecoderHDRInformation::EType::Unknown:	break;
		}
	}

	// If no HDR info is present, we assume sRGB
	return GetFullRange() ? MediaShaders::YuvToRgbRec709Unscaled : MediaShaders::YuvToRgbRec709Scaled;
}

bool IElectraTextureSampleBase::GetFullRange() const
{
	if (auto PinnedColorimetry = Colorimetry.Pin())
	{
		if (auto PinnedHDRInfo = HDRInfo.Pin())
		{
			if (PinnedHDRInfo->GetHDRType() != IVideoDecoderHDRInformation::EType::Unknown)
			{
				// For HDR we assume full range at all times
				return true;
			}
		}
		// SDR honors the flag in the MPEG stream...
		return (PinnedColorimetry->GetMPEGDefinition()->VideoFullRangeFlag != 0);
	}
	// SDR with no MPEG info is assumed to be video range
	return false;
}

FMatrix44f IElectraTextureSampleBase::GetSampleToRGBMatrix() const
{
	return YuvToRgbMtx;
}

FMatrix44f IElectraTextureSampleBase::GetGamutToXYZMatrix() const
{
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		switch (PinnedHDRInfo->GetHDRType())
		{
		case IVideoDecoderHDRInformation::EType::PQ10:
		case IVideoDecoderHDRInformation::EType::HLG10:
		case IVideoDecoderHDRInformation::EType::HDR10:		return GamutToXYZMatrix(EDisplayColorGamut::Rec2020_D65);
		case IVideoDecoderHDRInformation::EType::Unknown:	return GamutToXYZMatrix(EDisplayColorGamut::sRGB_D65);
		}
	}

	// If no HDR info is present, we assume sRGB
	return GamutToXYZMatrix(EDisplayColorGamut::sRGB_D65);
}

FVector2f IElectraTextureSampleBase::GetWhitePoint() const
{
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		if (auto ColorVolume = PinnedHDRInfo->GetMasteringDisplayColourVolume())
		{
			return FVector2f(ColorVolume->white_point_x, ColorVolume->white_point_y);
		}
	}
	// If no HDR info is present, we assume sRGB
	return FVector2f(UE::Color::GetWhitePoint(UE::Color::EWhitePoint::CIE1931_D65));
}

FVector2f IElectraTextureSampleBase::GetDisplayPrimaryRed() const
{
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		if (auto ColorVolume = PinnedHDRInfo->GetMasteringDisplayColourVolume())
		{
			return FVector2f(ColorVolume->display_primaries_x[0], ColorVolume->display_primaries_y[0]);
		}
	}
	// If no HDR info is present, we assume sRGB
	return FVector2f(0.64, 0.33);
}

FVector2f IElectraTextureSampleBase::GetDisplayPrimaryGreen() const
{
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		if (auto ColorVolume = PinnedHDRInfo->GetMasteringDisplayColourVolume())
		{
			return FVector2f(ColorVolume->display_primaries_x[1], ColorVolume->display_primaries_y[1]);
		}
	}
	// If no HDR info is present, we assume sRGB
	return FVector2f(0.30, 0.60);
}

FVector2f IElectraTextureSampleBase::GetDisplayPrimaryBlue() const
{
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		if (auto ColorVolume = PinnedHDRInfo->GetMasteringDisplayColourVolume())
		{
			return FVector2f(ColorVolume->display_primaries_x[2], ColorVolume->display_primaries_y[2]);
		}
	}
	// If no HDR info is present, we assume sRGB
	return FVector2f(0.15, 0.06);
}

UE::Color::EEncoding IElectraTextureSampleBase::GetEncodingType() const
{
	if (auto PinnedHDRInfo = HDRInfo.Pin())
	{
		switch (PinnedHDRInfo->GetHDRType())
		{
		case IVideoDecoderHDRInformation::EType::PQ10:
		case IVideoDecoderHDRInformation::EType::HDR10:		return UE::Color::EEncoding::ST2084;
		case IVideoDecoderHDRInformation::EType::Unknown:	return UE::Color::EEncoding::sRGB;
		case IVideoDecoderHDRInformation::EType::HLG10:
			{
			check(!"*** Implement support for HLG10 in UE::Color!");
			return UE::Color::EEncoding::sRGB;
			}
		}
	}
	// If no HDR info is present, we assume sRGB
	return UE::Color::EEncoding::sRGB;
}

#endif
