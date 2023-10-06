// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioCodec.h"

namespace Audio
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	struct UE_DEPRECATED(5.3, "Please use IAudioFormat/IStreamedCompressedInfo instead") FBackCompatCodec : public ICodec
	{
		// Static version.
		static const FCodecDetails& GetDetailsStatic();

		// Query.
		bool SupportsPlatform(FName InPlatformName) const override;
		const FCodecDetails& GetDetails() const override;

		// Factory for decoders
		FDecoderPtr CreateDecoder(
			IDecoder::FDecoderInputPtr InSrc,
			IDecoder::FDecoderOutputPtr InDst) override;	
	};

	struct UE_DEPRECATED(5.3, "Please use IAudioFormat/IStreamedCompressedInfo instead") FBackCompat : public IDecoder
	{
		IDecoder::FDecoderInputPtr Src = nullptr;
		IDecoder::FDecoderOutputPtr Dst = nullptr;
		FFormatDescriptorSection Desc;
		IDecoderOutput::FRequirements Reqs;
		TArray<int16> ResidualBuffer;
		uint32 FrameOffset = 0;

		FBackCompat(			
			IDecoder::FDecoderInputPtr InSrc,
			IDecoder::FDecoderOutputPtr InDst );

		bool HasError() const override
		{
			return false;
		}

		FDecodeReturn Decode(bool bLoop = false) override;

	private:
		bool bPreviousIsStreaming = false;
		bool bIsFirstDecode = true;
	};

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
