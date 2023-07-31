// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetNodes/SGraphNodeK2Default.h"
#include "Templates/SharedPointer.h"

class SGraphPin;
class UEdGraphPin;

class SGraphNodeCreateWidget : public SGraphNodeK2Default
{
public:
	// SGraphNode interface
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	// End of SGraphNode interface
};
