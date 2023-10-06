// Copyright Epic Games, Inc. All Rights Reserved.

#include "aac/AAC_AudioDecoder_Windows.h"

#ifdef ELECTRA_DECODERS_ENABLE_DX

#include "ElectraDecodersUtils.h"

#include "DX/AudioDecoderAAC_DX.h"

class FAACAudioDecoderFactoryWindows : public IElectraCodecFactory
{
public:
	virtual ~FAACAudioDecoderFactoryWindows()
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

		// At most 6 channels. Configurations 1-6 are supported.
		if (ci.NumberOfChannels > 6 || ci.ChannelConfiguration > 6)
		{
			return 0;
		}

		return 1;
	}
	
	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		IElectraAudioDecoderAAC_DX::GetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		check(SupportsFormat(InCodecFormat, false, InOptions));
		return IElectraAudioDecoderAAC_DX::Create(InOptions, InResourceDelegate);
	}

};


TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> FAACAudioDecoderWindows::CreateFactory()
{
	return MakeShared<FAACAudioDecoderFactoryWindows, ESPMode::ThreadSafe>();
}

#endif
