// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVConfig.h"
#include "Audio/AudioPacket.h"

#include "SimpleAV.h"

#include "SimpleAudio.generated.h"

UENUM(BlueprintType)
enum class ESimpleAudioCodec : uint8
{
	AAC = 0,
};

USTRUCT(BlueprintType)
struct FSimpleAudioPacket
{
	GENERATED_BODY()

public:
	FAudioPacket RawPacket;
};

UCLASS(Abstract)
class AVCODECSCORERHI_API USimpleAudioHelper : public USimpleAVHelper
{
	GENERATED_BODY()

public:
	static ESimpleAudioCodec GuessCodec(TSharedRef<FAVInstance> const& Instance);
};
