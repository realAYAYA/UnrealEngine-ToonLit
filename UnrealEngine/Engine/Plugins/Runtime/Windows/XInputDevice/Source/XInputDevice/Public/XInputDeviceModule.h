// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IInputDeviceModule.h"

/**
 * Input Device Module for XInput, an input API for Windows.
 */
class FXInputDeviceModule final : public IInputDeviceModule
{
public:
	//~ Begin IInputDeviceModule interface
	virtual TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	virtual TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, FInputDeviceCreationParameters InParameters) override;
	//~ End IInputDeviceModule interface
};
