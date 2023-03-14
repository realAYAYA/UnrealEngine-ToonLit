// Copyright Epic Games, Inc. All Rights Reserved.

#include "libav_Decoder_H264.h"

/***************************************************************************************************************************************************/

#if WITH_LIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
}

bool ILibavDecoderH264::IsAvailable()
{
    return ILibavDecoderVideoCommon::IsAvailable(AV_CODEC_ID_H264);
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderH264::Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
    return ILibavDecoderVideoCommon::Create(AV_CODEC_ID_H264, InVideoResourceAllocator, InOptions);
}

#else

bool ILibavDecoderH264::IsAvailable()
{
	// Call common method to have it print an appropriate not-available message.
	ILibavDecoder::LogLibraryNeeded();
    return false;
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderH264::Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
    return nullptr;
}

#endif
