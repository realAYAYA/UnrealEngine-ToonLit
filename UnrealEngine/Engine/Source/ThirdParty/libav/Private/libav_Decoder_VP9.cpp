// Copyright Epic Games, Inc. All Rights Reserved.

#include "libav_Decoder_VP9.h"

/***************************************************************************************************************************************************/

#if WITH_LIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
}

bool ILibavDecoderVP9::IsAvailable()
{
    return ILibavDecoderVideoCommon::IsAvailable(AV_CODEC_ID_VP9);
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderVP9::Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
    return ILibavDecoderVideoCommon::Create(AV_CODEC_ID_VP9, InVideoResourceAllocator, InOptions);
}

#else

bool ILibavDecoderVP9::IsAvailable()
{
	// Call common method to have it print an appropriate not-available message.
	ILibavDecoder::LogLibraryNeeded();
    return false;
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderVP9::Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
    return nullptr;
}

#endif
