// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Decoders/VideoDecoderVT.h"
#include "Video/Resources/Metal/VideoResourceMetal.h"

namespace Internal
{
    void VTDecompressionOutputCallback(void* Decoder, void* Params, OSStatus Status, VTDecodeInfoFlags InfoFlags, CVImageBufferRef ImageBuffer, CMTime Timestamp, CMTime Duration)
    {
        static_cast<TVideoDecoderVT<FVideoResourceMetal>*>(Decoder)->HandleFrame(Params, Status, InfoFlags, ImageBuffer, Timestamp, Duration);
    }
}
