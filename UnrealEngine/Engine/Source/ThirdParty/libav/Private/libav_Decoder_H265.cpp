// Copyright Epic Games, Inc. All Rights Reserved.

#include "libav_Decoder_H265.h"

/***************************************************************************************************************************************************/

#if WITH_LIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
}

bool ILibavDecoderH265::IsAvailable()
{
    return ILibavDecoderVideoCommon::IsAvailable(AV_CODEC_ID_HEVC);
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderH265::Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
    return ILibavDecoderVideoCommon::Create(AV_CODEC_ID_HEVC, InVideoResourceAllocator, InOptions);
}

#else

bool ILibavDecoderH265::IsAvailable()
{
	// Call common method to have it print an appropriate not-available message.
	ILibavDecoder::LogLibraryNeeded();
    return false;
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderH265::Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
    return nullptr;
}

#endif
