// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "libav_Decoder_Common_Video.h"

class ILibavDecoderH264
{
public:
	// Checks if the H.264 decoder is available.
	static LIBAV_API bool IsAvailable();

	// Create an instance of the decoder. If not available nullptr is returned.
	static LIBAV_API TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> Create(ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions);
};
