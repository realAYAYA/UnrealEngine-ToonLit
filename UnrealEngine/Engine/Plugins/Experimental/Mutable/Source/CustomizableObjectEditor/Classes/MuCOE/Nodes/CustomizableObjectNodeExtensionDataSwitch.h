// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeSwitchBase.h"
#include "MuCOE/ICustomizableObjectExtensionNode.h"

#include "CustomizableObjectNodeExtensionDataSwitch.generated.h"

UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeExtensionDataSwitch
	: public UCustomizableObjectNodeSwitchBase
	, public ICustomizableObjectExtensionNode
{
	GENERATED_BODY()

public:

	//~Begin UCustomizableObjectNode interface
	virtual bool IsAffectedByLOD() const override;
	//~End UCustomizableObjectNode interface

	//~Begin UCustomizableObjectNodeSwitchBase interface
	virtual bool ShouldAddToContextMenu(FText& OutCategory) const override;
	virtual FString GetPinPrefix() const override;
	//~End UCustomizableObjectNodeSwitchBase interface

	//~Begin ICustomizableObjectExtensionNode interface
	mu::NodeExtensionDataPtr GenerateMutableNode(class FExtensionDataCompilerInterface& InCompilerInterface) const override;
	//~End ICustomizableObjectExtensionNode interface
};
