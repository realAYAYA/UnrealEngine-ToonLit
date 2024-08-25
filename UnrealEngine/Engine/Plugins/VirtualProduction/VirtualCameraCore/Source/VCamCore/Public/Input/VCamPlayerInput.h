// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedPlayerInput.h"
#include "VCamInputDeviceConfig.h"
#include "VCamPlayerInput.generated.h"

/**
 * Receives raw input from input processor (or player controller in user code).
 * Filters input
 */
UCLASS()
class VCAMCORE_API UVCamPlayerInput : public UEnhancedPlayerInput
{
	GENERATED_BODY()
public:

	//~ Begin UEnhancedPlayerInput Interface
	virtual bool InputKey(const FInputKeyParams& Params) override;
	//~ End UEnhancedPlayerInput Interface

	const FVCamInputDeviceConfig& GetInputSettings() const { return InputDeviceSettings; }
	void SetInputSettings(const FVCamInputDeviceConfig& Input);

private:

	/** The device settings governing what we do with received input, e.g. filtering depending on input device, logging, etc. */
	UPROPERTY(Transient)
	FVCamInputDeviceConfig InputDeviceSettings;
};
