// Copyright Epic Games, Inc. All Rights Reserved.

#include "aac/AAC_AudioDecoder_Apple.h"

#ifdef ELECTRA_DECODERS_ENABLE_APPLE

#include "ElectraDecodersUtils.h"

#include "aac/AudioDecoderAAC_Apple.h"

class FAACAudioDecoderFactoryApple : public IElectraCodecFactory
{
public:
	virtual ~FAACAudioDecoderFactoryApple()
	{}

	int32 SupportsFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) const override
	{
		// Encoder? Not supported here!
		if (bInEncoder)
		{
			return 0;
		}

		ElectraDecodersUtil::FMimeTypeAudioCodecInfo ci;
		// Codec?
		if (ElectraDecodersUtil::ParseCodecMP4A(ci, InCodecFormat))
		{
			// Ok.
		}
		// Mime type?
		else if (ElectraDecodersUtil::ParseMimeTypeWithCodec(ci, InCodecFormat))
		{
		}
		else
		{
			return 0;
		}

		// Check for the correct object type. Realistically this should be set.
		if (ci.ObjectType != 0x40)
		{
			return 0;
		}

		// AAC-LC, AAC-HE (SBR), AAC-HEv2 (PS) ?
		if (ci.Profile != 2 && ci.Profile != 5 && ci.Profile != 29)
		{
			return 0;
		}

		ci.ChannelConfiguration = (uint32) ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("channel_configuration"), 0);
		ci.NumberOfChannels = (int32) ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("num_channels"), 0);

		if (!(ci.NumberOfChannels <= 8 && (
			ci.ChannelConfiguration == 1 ||		// Mono
			ci.ChannelConfiguration == 2 ||		// Stereo
			ci.ChannelConfiguration == 3 ||		// L/C/R
			ci.ChannelConfiguration == 6 ||		// 5.1
			ci.ChannelConfiguration == 12)))	// 7.1
		{
			return 0;
		}

		return 1;
	}

	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		IElectraAudioDecoderAAC_Apple::GetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		check(SupportsFormat(InCodecFormat, false, InOptions));
		return IElectraAudioDecoderAAC_Apple::Create(InOptions, InResourceDelegate);
	}

};


TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> FAACAudioDecoderApple::CreateFactory()
{
	return MakeShared<FAACAudioDecoderFactoryApple, ESPMode::ThreadSafe>();
}

#endif
