// Copyright Epic Games, Inc. All Rights Reserved.

#include "h264/H264_VideoDecoder_Linux.h"

#ifdef ELECTRA_DECODERS_ENABLE_LINUX
#include "h264/VideoDecoderH264_Linux.h"

#include "ElectraDecodersUtils.h"

#include "libav_Decoder_Common.h"
#include "libav_Decoder_H264.h"

class FH264VideoDecoderFactoryLinux : public IElectraCodecFactory
{
public:
	virtual ~FH264VideoDecoderFactoryLinux()
	{}

	int32 SupportsFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) const override
	{
		// Encoder? Not supported here!
		if (bInEncoder)
		{
			return 0;
		}

		// Codec available?
		if (!ILibavDecoderH264::IsAvailable())
		{
			return 0;
		}

		// Get properties that cannot be passed with the codec string alone.
		uint32 Width = (uint32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("width"), 0);
		uint32 Height = (uint32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("height"), 0);
		//int64 bps = ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("bitrate"), 0);
		double fps = ElectraDecodersUtil::GetVariantValueSafeDouble(InOptions, TEXT("fps"), 0.0);

		ElectraDecodersUtil::FMimeTypeVideoCodecInfo ci;
		// Codec?
		if (ElectraDecodersUtil::ParseCodecH264(ci, InCodecFormat))
		{
			// Ok.
		}
		// Mime type?
		else if (ElectraDecodersUtil::ParseMimeTypeWithCodec(ci, InCodecFormat))
		{
			// Note: This should ideally have the resolution.
		}
		else
		{
			return 0;
		}

		// Check if supported.
		TArray<IElectraVideoDecoderH264_Linux::FSupportedConfiguration> Configs;
		IElectraVideoDecoderH264_Linux::PlatformGetSupportedConfigurations(Configs);
		bool bSupported = false;
		for(int32 i=0; i<Configs.Num(); ++i)
		{
			const IElectraVideoDecoderH264_Linux::FSupportedConfiguration& Cfg = Configs[i];
			if (Cfg.Profile == ci.Profile)
			{
				if (ci.Level > Cfg.Level)
				{
					continue;
				}
				if ((Width > Cfg.Width && Cfg.Width) || (Height > Cfg.Height && Cfg.Height))
				{
					continue;
				}
				if (fps > 0.0 && Cfg.FramesPerSecond && (int32)fps > Cfg.FramesPerSecond)
				{
					continue;
				}
				if (Cfg.Num16x16Macroblocks && ((Align(Width, 16) * Align(Height, 16)) / 256) > Cfg.Num16x16Macroblocks)
				{
					continue;
				}
				bSupported = true;
				break;
			}
		}

		return bSupported ? 1 : 0;
	}


	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		IElectraVideoDecoderH264_Linux::GetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		check(SupportsFormat(InCodecFormat, false, InOptions));
		return IElectraVideoDecoderH264_Linux::Create(InOptions, InResourceDelegate);
	}

};


TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> FH264VideoDecoderLinux::CreateFactory()
{
	return MakeShared<FH264VideoDecoderFactoryLinux, ESPMode::ThreadSafe>();
}

#endif
