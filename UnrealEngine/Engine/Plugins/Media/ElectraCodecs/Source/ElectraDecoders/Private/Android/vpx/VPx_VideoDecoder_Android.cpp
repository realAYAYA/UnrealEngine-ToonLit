// Copyright Epic Games, Inc. All Rights Reserved.

#include "vpx/VPx_VideoDecoder_Android.h"
#include "vpx/VideoDecoderVPx_Android.h"

#include "ElectraDecodersUtils.h"


class FVPxVideoDecoderFactoryAndroid : public IElectraCodecFactory
{
public:
	virtual ~FVPxVideoDecoderFactoryAndroid()
	{}

	int32 SupportsFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) const override
	{
		// Encoder? Not supported here!
		if (bInEncoder)
		{
			return 0;
		}

		if (InCodecFormat.StartsWith(TEXT("vp08"), ESearchCase::IgnoreCase))
		{
			return 1;
		}
		else if (InCodecFormat.StartsWith(TEXT("vp09"), ESearchCase::IgnoreCase))
		{
			ElectraDecodersUtil::FMimeTypeVideoCodecInfo ci;
			if (ElectraDecodersUtil::ParseCodecVP9(ci, InCodecFormat, ElectraDecodersUtil::GetVariantValueUInt8Array(InOptions, TEXT("$vpcC_box"))))
			{
				return 1;
			}
		}
		return 0;
	}


	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		IElectraVideoDecoderVPx_Android::GetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		check(SupportsFormat(InCodecFormat, false, InOptions));
		return IElectraVideoDecoderVPx_Android::Create(InOptions, InResourceDelegate);
	}

};


TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> FVPxVideoDecoderAndroid::CreateFactory()
{
	return MakeShared<FVPxVideoDecoderFactoryAndroid, ESPMode::ThreadSafe>();
}
