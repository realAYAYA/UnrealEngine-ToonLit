// Copyright Epic Games, Inc. All Rights Reserved.

#include "vpx/VPx_VideoDecoder_Linux.h"

#ifdef ELECTRA_DECODERS_ENABLE_LINUX
#include "vpx/VideoDecoderVPx_Linux.h"

#include "ElectraDecodersUtils.h"

#include "libav_Decoder_Common.h"
#include "libav_Decoder_VP8.h"
#include "libav_Decoder_VP9.h"

class FVPxVideoDecoderFactoryLinux : public IElectraCodecFactory
{
public:
	virtual ~FVPxVideoDecoderFactoryLinux()
	{}

	int32 SupportsFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) const override
	{
		// Encoder? Not supported here!
		if (bInEncoder)
		{
			return 0;
		}

		// Get properties that cannot be passed with the codec string alone.
		uint32 Width = (uint32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("width"), 0);
		uint32 Height = (uint32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("height"), 0);
		//int64 bps = ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("bitrate"), 0);
		//double fps = ElectraDecodersUtil::GetVariantValueSafeDouble(InOptions, TEXT("fps"), 0.0);

		ElectraDecodersUtil::FMimeTypeVideoCodecInfo ci;
		// VP9 Codec?
		if (ElectraDecodersUtil::ParseCodecVP9(ci, InCodecFormat, ElectraDecodersUtil::GetVariantValueUInt8Array(InOptions, TEXT("$vpcC_box"))))
		{
			// Codec available?
			if (!ILibavDecoderVP9::IsAvailable())
			{
				return 0;
			}
		}
		// VP8 Codec?
		else if (ElectraDecodersUtil::ParseCodecVP8(ci, InCodecFormat, ElectraDecodersUtil::GetVariantValueUInt8Array(InOptions, TEXT("$vpcC_box"))))
		{
			// Codec available?
			if (!ILibavDecoderVP8::IsAvailable())
			{
				return 0;
			}
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

		// See if there are values provided we can check against.
		if ((Width && Width < 128) || (Height && Height < 128))
		{
			return 0;
		}
		else if ((Width && Width > 8192) || (Height && Height > 8192))
		{
			return 0;
		}

		return 5;
	}


	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		IElectraVideoDecoderVPx_Linux::GetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		return IElectraVideoDecoderVPx_Linux::Create(InOptions, InResourceDelegate);
	}

};


TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> FVPxVideoDecoderLinux::CreateFactory()
{
	return MakeShared<FVPxVideoDecoderFactoryLinux, ESPMode::ThreadSafe>();
}

#endif
