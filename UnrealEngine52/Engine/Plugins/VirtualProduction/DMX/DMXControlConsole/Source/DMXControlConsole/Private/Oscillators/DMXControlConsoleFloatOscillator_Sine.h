// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Oscillators/DMXControlConsoleFloatOscillator.h"

#include "DMXControlConsoleFloatOscillator_Sine.generated.h"


/** Generates DMX Signals in a Sine Pattern */
UCLASS(Meta = (DisplayName = "Sine Wave"))
class DMXCONTROLCONSOLE_API UDMXControlConsoleFloatOscillator_Sine
	: public UDMXControlConsoleFloatOscillator
{
	GENERATED_BODY()

public:
	/** The frequency of the sine wave, in Hz */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", UIMin = "0", UIMax = "120"), Category = "Sine Wave Oscillator")
	float FrequencyHz = .25f;

	/** The amplitude of the sine wave, in the range [0, 1] */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"), Category = "Sine Wave Oscillator")
	float Amplitude = 1.f;

	/** The offset of the sine wave, in the range [0, 1] */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"), Category = "Sine Wave Oscillator")
	float Offset = 0.5f;

	//~ Begin DMXControlConsoleFloatOscillator interface
	virtual float GetNormalizedValue_Implementation(float DeltaTime) override;
	//~ End DMXControlConsoleFloatOscillator interface

private:
	/** The current time, used to generate the sine wave */
	double CurrentTime = 0.f;
};
