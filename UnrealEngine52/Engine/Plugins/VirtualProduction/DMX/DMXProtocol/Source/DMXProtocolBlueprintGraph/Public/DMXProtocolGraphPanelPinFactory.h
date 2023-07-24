// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

class SGraphPin;

class DMXPROTOCOLBLUEPRINTGRAPH_API FDMXProtocolGraphPanelPinFactory : public FGraphPanelPinFactory
{
	//~ Begin FGraphPanelPinFactory implementation
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;
	//~ End FGraphPanelPinFactory implementation
};
