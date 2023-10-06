// Copyright Epic Games, Inc. All Rights Reserved.

#include "libav_Decoder_VP8.h"

/***************************************************************************************************************************************************/

#if WITH_LIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
}

bool ILibavDecoderVP8::IsAvailable()
{
    return ILibavDecoderVideoCommon::IsAvailable(AV_CODEC_ID_VP8);
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderVP8::Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
    return ILibavDecoderVideoCommon::Create(AV_CODEC_ID_VP8, InVideoResourceAllocator, InOptions);
}

#else

bool ILibavDecoderVP8::IsAvailable()
{
	// Call common method to have it print an appropriate not-available message.
	ILibavDecoder::LogLibraryNeeded();
    return false;
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderVP8::Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
    return nullptr;
}

#endif
