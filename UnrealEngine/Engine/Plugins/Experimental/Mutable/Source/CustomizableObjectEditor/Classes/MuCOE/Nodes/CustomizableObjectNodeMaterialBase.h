// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Misc/AssertionMacros.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMaterialBase.generated.h"

class UEdGraphPin;
class UObject;



UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialBase : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// Own interface
	virtual TArray<class UCustomizableObjectLayout*> GetLayouts() { return TArray<class UCustomizableObjectLayout*>(); }

	// This method should be overidden in all derived classes
	virtual UEdGraphPin* OutputPin() const
	{
		check(false); 
		return nullptr;
	}

	/** Allow multiple connections to NodeCopyMaterial but only one connection to a NodeObject. */
	bool ShouldBreakExistingConnections(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const override;
};

