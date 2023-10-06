// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolTypes.h"

#include "UObject/Object.h"

#include "DMXControlConsoleFloatOscillator.generated.h"


/** A value Oscillator that can be used in the DMX Control Console. Outputs float (normalized values) */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, Abstract)
class DMXCONTROLCONSOLE_API UDMXControlConsoleFloatOscillator
	: public UObject
{
	GENERATED_BODY()

public:
	/** Gets a normalized value that is sent as DMX */
	UFUNCTION(BlueprintNativeEvent, Category = "DMX")
	float GetNormalizedValue(float DeltaTime);
	virtual float GetNormalizedValue_Implementation(float DeltaTime) { return 0.f; };
};
