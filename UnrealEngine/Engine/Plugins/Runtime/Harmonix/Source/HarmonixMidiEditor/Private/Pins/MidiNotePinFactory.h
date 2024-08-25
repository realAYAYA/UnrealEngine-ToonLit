// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"

/**
* Factory class that creates a custom Graph Pin for FMidiNote struct pins.
*/
class FMidiNotePinFactory : public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;
};