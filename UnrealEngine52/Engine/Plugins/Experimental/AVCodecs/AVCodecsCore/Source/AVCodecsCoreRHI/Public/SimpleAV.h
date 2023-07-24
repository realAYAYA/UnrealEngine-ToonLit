// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVConfig.h"

#include "SimpleAV.generated.h"

UENUM(BlueprintType)
enum class ESimpleAVPreset : uint8
{
	Default,
	
	UltraLowQuality,
	LowQuality,
	HighQuality,
	Lossless,
};

UCLASS(Abstract)
class AVCODECSCORERHI_API USimpleAVHelper : public UObject
{
	GENERATED_BODY()

public:
	static EAVPreset ConvertPreset(ESimpleAVPreset From);
};
