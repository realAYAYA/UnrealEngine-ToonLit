// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderVP8.h"


// Should be defined in AVEncoder_{Platform}.Build.cs
// If not there is no native support and we have to provide a default implementation
// that registers nothing.
#ifndef CODEC_HAVE_NATIVE_VP8_DECODER
#define CODEC_HAVE_NATIVE_VP8_DECODER 0
#endif


#if CODEC_HAVE_NATIVE_VP8_DECODER == 0

namespace AVEncoder
{

void FVideoDecoderVP8::Register(FVideoDecoderFactory& InFactory)
{
}

} // namespace AVEncoder

#endif
