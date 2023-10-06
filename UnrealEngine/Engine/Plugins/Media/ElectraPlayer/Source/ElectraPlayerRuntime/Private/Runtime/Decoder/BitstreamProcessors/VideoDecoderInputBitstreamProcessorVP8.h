// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Decoder/VideoDecoderInputBitstreamProcessor.h"

namespace Electra
{

class IVideoDecoderInputBitstreamProcessorVP8
{
public:
	static TSharedPtr<IVideoDecoderInputBitstreamProcessor, ESPMode::ThreadSafe> Create(const FString& InCodec, const TMap<FString, FVariant>& InDecoderConfigOptions);
};

} // namespace Electra
