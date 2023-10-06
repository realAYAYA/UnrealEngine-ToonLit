// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsPlatformElectraDecoders.h"
#include "IElectraCodecRegistry.h"

#include "h264/H264_VideoDecoder_Windows.h"
#include "h265/H265_VideoDecoder_Windows.h"
#include "aac/AAC_AudioDecoder_Windows.h"

namespace PlatformElectraDecodersWindows
{
	static TArray<TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe>> Factories;
}


void FPlatformElectraDecodersWindows::Startup()
{
}

void FPlatformElectraDecodersWindows::Shutdown()
{
	PlatformElectraDecodersWindows::Factories.Empty();
}

void FPlatformElectraDecodersWindows::RegisterWithCodecFactory(IElectraCodecRegistry* InCodecFactoryToRegisterWith)
{
	check(InCodecFactoryToRegisterWith);
	if (InCodecFactoryToRegisterWith)
	{
#ifdef ELECTRA_DECODERS_ENABLE_DX
		auto RegisterFactory = [&](TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> InFactory) -> void
		{
			if (InFactory.IsValid())
			{
				PlatformElectraDecodersWindows::Factories.Emplace(InFactory);
				InCodecFactoryToRegisterWith->AddCodecFactory(InFactory);
			}
		};

		// H.264 video decoder
		RegisterFactory(FH264VideoDecoderWindows::CreateFactory());
		// H.265 video decoder
		RegisterFactory(FH265VideoDecoderWindows::CreateFactory());
		// AAC audio decoder
		RegisterFactory(FAACAudioDecoderWindows::CreateFactory());
#endif
	}
}
