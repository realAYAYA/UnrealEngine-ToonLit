// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxUtils.h"

#include "HAL/IConsoleManager.h"
#include "RivermaxLog.h"
#include "RivermaxTypes.h"


namespace UE::RivermaxCore::Private::Utils
{
	FString PixelFormatToSamplingDesc(ESamplingType SamplingType)
	{
		switch (SamplingType)
		{
			case ESamplingType::RGB_8bit:
			case ESamplingType::RGB_10bit:
			case ESamplingType::RGB_12bit:
			case ESamplingType::RGB_16bit:
			case ESamplingType::RGB_16bitFloat:
			{
				return FString(TEXT("RGB"));
			}
			case ESamplingType::YUV422_8bit:
			case ESamplingType::YUV422_10bit:
			case ESamplingType::YUV422_12bit:
			case ESamplingType::YUV422_16bit:
			case ESamplingType::YUV422_16bitFloat:
			{
				return FString(TEXT("YCbCr-4:2:2"));
			}
			case ESamplingType::YUV444_8bit:
			case ESamplingType::YUV444_10bit:
			case ESamplingType::YUV444_12bit:
			case ESamplingType::YUV444_16bit:
			case ESamplingType::YUV444_16bitFloat:
			{
				return FString(TEXT("YCbCr-4:4:4"));
			}
			default:
			{
				checkNoEntry();
				return FString();
			}
		}
	}

	FString PixelFormatToBitDepth(ESamplingType SamplingType)
	{
		switch (SamplingType)
		{
		case ESamplingType::RGB_8bit:
		case ESamplingType::YUV422_8bit:
		case ESamplingType::YUV444_8bit:
		{
			return FString(TEXT("8"));
		}
		case ESamplingType::RGB_10bit:
		case ESamplingType::YUV422_10bit:
		case ESamplingType::YUV444_10bit:
		{
			return FString(TEXT("10"));
		}
		case ESamplingType::RGB_12bit:
		case ESamplingType::YUV422_12bit:
		case ESamplingType::YUV444_12bit:
		{
			return FString(TEXT("12"));
		}
		case ESamplingType::RGB_16bit:
		case ESamplingType::YUV422_16bit:
		case ESamplingType::YUV444_16bit:
		{
			return FString(TEXT("16"));
		}
		case ESamplingType::RGB_16bitFloat:
		case ESamplingType::YUV422_16bitFloat:
		case ESamplingType::YUV444_16bitFloat:
		{
			return FString(TEXT("16f"));
		}
		default:
		{
			checkNoEntry();
			return FString();
		}
		}
	}

	void StreamOptionsToSDPDescription(const UE::RivermaxCore::FRivermaxOutputStreamOptions& Options, float RateMultiplier, FAnsiStringBuilderBase& OutSDPDescription)
	{
		// Basic SDP string creation from a set of options. At some point, having a proper SDP loader / creator would be useful.
		// Refer to https://datatracker.ietf.org/doc/html/rfc4570

		// Apply desired multiplier and convert fractional frame rate to be represented over 1001 for sdp compliance.
		FFrameRate DesiredRate = Options.FrameRate;
		const double DecimalDesiredRate = Options.FrameRate.AsDecimal();
		const bool bIsNonStandardFractionalFrameRate = (DesiredRate.Denominator != 1001 && !FMath::IsNearlyZero(FMath::Frac(DecimalDesiredRate)));
		if (bIsNonStandardFractionalFrameRate)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Fractional frame rates must be described using a denominator of 1001. Converting it for stream creation."));
		}

		FString FrameRateDescription;
		if (!FMath::IsNearlyEqual(RateMultiplier, 1.0f) || bIsNonStandardFractionalFrameRate)
		{
			const double NewRateDecimal = DecimalDesiredRate * RateMultiplier * 1001;
			DesiredRate.Numerator = FMath::RoundToInt32(NewRateDecimal);
			DesiredRate.Denominator = 1001;
		}
		
		if(DesiredRate.Denominator == 1001)
		{
			FrameRateDescription = FString::Printf(TEXT("%u/%u"), DesiredRate.Numerator, DesiredRate.Denominator);
		}
		else
		{
			FrameRateDescription = FString::Printf(TEXT("%u"), FMath::RoundToInt32(DesiredRate.AsDecimal()));
		}

		constexpr int32 MulticastTTL = 64;
		OutSDPDescription.Appendf("v=0\n");
		OutSDPDescription.Appendf("s=SMPTE ST2110 20 streams\n");
		OutSDPDescription.Appendf("t=0 0\n");
		OutSDPDescription.Appendf("m=video %d RTP/AVP 96\n", Options.Port);
		OutSDPDescription.Appendf("c=IN IP4 %S/%d\n", *Options.StreamAddress, MulticastTTL);
		OutSDPDescription.Appendf("a=source-filter: incl IN IP4 %S %S\n", *Options.StreamAddress, *Options.InterfaceAddress);
		OutSDPDescription.Appendf("a=rtpmap:96 raw/90000\n");
		OutSDPDescription.Appendf("a=fmtp: 96 sampling=%S; width=%d; height=%d; exactframerate=%S; depth=%S; TCS=SDR; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017; TP=2110TPN;\n"
			, *PixelFormatToSamplingDesc(Options.PixelFormat)
			, Options.AlignedResolution.X
			, Options.AlignedResolution.Y
			, *FrameRateDescription
			, *PixelFormatToBitDepth(Options.PixelFormat));
	}

	uint32 TimestampToFrameNumber(uint32 Timestamp, const FFrameRate& FrameRate)
	{
		using namespace UE::RivermaxCore::Private::Utils;
		const double MediaFrameTime = Timestamp / MediaClockSampleRate;
		const uint32 FrameNumber = FMath::Floor(MediaFrameTime * FrameRate.AsDecimal());
		return FrameNumber;
	}
}