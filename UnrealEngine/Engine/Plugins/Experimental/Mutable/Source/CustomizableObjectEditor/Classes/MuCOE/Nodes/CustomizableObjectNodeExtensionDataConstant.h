// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeExtensionDataConstant.generated.h"

/**
 * A base class for nodes that output a constant ExtensionData.
 *
 * This is typically used to import an Unreal asset into the Customizable Object graph.
 */
UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeExtensionDataConstant : public UCustomizableObjectNode
{
	GENERATED_BODY()

public:
	virtual bool IsAffectedByLOD() const override { return false; }
};

