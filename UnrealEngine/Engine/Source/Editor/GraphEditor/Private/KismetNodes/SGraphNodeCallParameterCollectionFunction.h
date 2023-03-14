// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetNodes/SGraphNodeK2Default.h"
#include "Templates/SharedPointer.h"

class SGraphPin;
class UEdGraphPin;

class SGraphNodeCallParameterCollectionFunction : public SGraphNodeK2Default
{
protected:

	// SGraphNode interface
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	// End of SGraphNode interface
};
