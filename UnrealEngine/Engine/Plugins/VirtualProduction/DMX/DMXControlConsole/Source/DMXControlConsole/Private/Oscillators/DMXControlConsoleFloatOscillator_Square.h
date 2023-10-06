// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Oscillators/DMXControlConsoleFloatOscillator.h"

#include "DMXControlConsoleFloatOscillator_Square.generated.h"


/** Generates DMX Signals in a Square Wave Pattern */
UCLASS(Meta = (DisplayName = "Square Wave"))
class DMXCONTROLCONSOLE_API UDMXControlConsoleFloatOscillator_Square
	: public UDMXControlConsoleFloatOscillator
{
	GENERATED_BODY()

public:
	/** The frequency of the square wave, in Hz */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", UIMin = "0"), Category = "Sine Wave Oscillator")
	float FrequencyHz = 1.f;

	/** The amplitude of the square wave, in the range [0, 1] */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"), Category = "Sine Wave Oscillator")
	float Amplitude = 1.f;

	/** The offset of the square wave, in the range [0, 1] */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"), Category = "Sine Wave Oscillator")
	float Offset = 0.5f;

	//~ Begin DMXControlConsoleFloatOscillator interface
	virtual float GetNormalizedValue_Implementation(float DeltaTime) override;
	//~ End DMXControlConsoleFloatOscillator interface

private:
	/** The current time, used to generate the square wave */
	double CurrentTime = 0.f;
};
