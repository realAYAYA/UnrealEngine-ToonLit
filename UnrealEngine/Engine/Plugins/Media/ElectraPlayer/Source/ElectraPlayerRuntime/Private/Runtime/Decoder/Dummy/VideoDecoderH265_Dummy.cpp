// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"

#if ELECTRA_PLATFORM_HAS_H265_DECODER == 0

#include "Decoder/VideoDecoderH265.h"

namespace Electra
{
	class FVideoDecoderH265 : public IVideoDecoderH265
	{
	public:
		static bool Startup(const FParamDict& Options)
		{ return true; }
		static void Shutdown()
		{ }
		static bool GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter)
		{ return false; }
		static FVideoDecoderH265* Create()
		{ return nullptr; }
		virtual ~FVideoDecoderH265() = default;
		virtual void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) override
		{ }
		virtual void Open(const FInstanceConfiguration& InConfig) override
		{ }
		virtual void Close() override
		{ }
		virtual void DrainForCodecChange() override
		{ }
		virtual void SetMaximumDecodeCapability(int32 MaxTier, int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions) override
		{ }
		virtual void SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& ResourceDelegate) override
		{ }
		virtual void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) override
		{ }
		virtual void AUdataPushAU(FAccessUnit* AccessUnit) override
		{ }
		virtual void AUdataPushEOD() override
		{ }
		virtual void AUdataClearEOD() override
		{ }
		virtual void AUdataFlushEverything() override
		{ }
		virtual void SetAUInputBufferListener(IAccessUnitBufferListener* Listener) override
		{ }
		virtual void SetReadyBufferListener(IDecoderOutputBufferListener* Listener) override
		{ }
	#if PLATFORM_ANDROID
		virtual void Android_UpdateSurface(const TSharedPtr<IOptionPointerValueContainer>& Surface) override
		{ }
	#endif
	};

	IVideoDecoderH265::FInstanceConfiguration::FInstanceConfiguration()
	{ }

	IVideoDecoderH265* IVideoDecoderH265::Create()
	{
		return FVideoDecoderH265::Create();
	}
	bool IVideoDecoderH265::Startup(const FParamDict& Options)
	{ 
		return FVideoDecoderH265::Startup(Options);
	}
	void IVideoDecoderH265::Shutdown()
	{ 
		FVideoDecoderH265::Shutdown();
	}
	bool IVideoDecoderH265::GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter)
	{ 
		return FVideoDecoderH265::GetStreamDecodeCapability(OutResult, InStreamParameter);
	}


}
#endif
