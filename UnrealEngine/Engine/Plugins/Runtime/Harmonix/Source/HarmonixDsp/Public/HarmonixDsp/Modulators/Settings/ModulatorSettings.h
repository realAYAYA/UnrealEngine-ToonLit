// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/EnumRange.h"

#include "ModulatorSettings.generated.h"

UENUM(BlueprintType)
enum class EModulatorTarget : uint8
{
	StartPoint	UMETA(Json = "Start Point"),
	Pitch		UMETA(Json = "Pitch"),
	Num			UMETA(Hidden),
	None		UMETA(Hidden, Json = "None")
};

USTRUCT(BlueprintType)
struct HARMONIXDSP_API FModulatorSettings
{
	GENERATED_BODY()

public:
	FModulatorSettings();

	UPROPERTY(EditDefaultsOnly, Category="Harmonix|DSP")
	EModulatorTarget Target = EModulatorTarget::None;

	UPROPERTY(EditDefaultsOnly, Category = "Harmonix|DSP", Meta = (UIMin = "0.0", UIMax = "1000000.0", ClampMin = "0.0", ClampMax = "1000000.0"))
	float Range = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Harmonix|DSP", Meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float Depth = 0.5f;
};

USTRUCT()
struct FModulatorSettingsArray
{
	GENERATED_BODY()

	UPROPERTY()
	FModulatorSettings Array[2];
};