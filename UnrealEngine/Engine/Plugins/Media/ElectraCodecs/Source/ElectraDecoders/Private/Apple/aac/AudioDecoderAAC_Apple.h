// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IElectraDecoder.h"
#include "IElectraDecoderResourceDelegate.h"

#ifdef ELECTRA_DECODERS_ENABLE_APPLE

class IElectraAudioDecoderAAC_Apple : public IElectraDecoder
{
public:
	static void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions);
	static TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> Create(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate);

	virtual ~IElectraAudioDecoderAAC_Apple() = default;
};

#endif
