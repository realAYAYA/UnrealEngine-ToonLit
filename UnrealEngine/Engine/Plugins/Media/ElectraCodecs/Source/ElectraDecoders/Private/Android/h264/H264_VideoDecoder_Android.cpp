// Copyright Epic Games, Inc. All Rights Reserved.

#include "h264/H264_VideoDecoder_Android.h"
#include "h264/VideoDecoderH264_Android.h"

#include "ElectraDecodersUtils.h"


class FH264VideoDecoderFactoryAndroid : public IElectraCodecFactory
{
public:
	virtual ~FH264VideoDecoderFactoryAndroid()
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

		// Baseline, Main or High profile?
		if (ci.Profile != 66 && ci.Profile != 77 && ci.Profile != 100)
		{
			return 0;
		}

		// Limit to 1080p for now.
		if ((Width && Width > 1920) || (Height && Height > 1088))
		{
			return 0;
		}

		return 1;
	}


	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		IElectraVideoDecoderH264_Android::GetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		check(SupportsFormat(InCodecFormat, false, InOptions));
		return IElectraVideoDecoderH264_Android::Create(InOptions, InResourceDelegate);
	}

};


TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> FH264VideoDecoderAndroid::CreateFactory()
{
	return MakeShared<FH264VideoDecoderFactoryAndroid, ESPMode::ThreadSafe>();
}

