// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeExtensionData.h"
#include "UObject/Interface.h"

#include "ICustomizableObjectExtensionNode.generated.h"

class FExtensionDataCompilerInterface;

/** Customizable Object graph nodes that output ExtensionData should implement this interface */
UINTERFACE()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectExtensionNode : public UInterface
{
	GENERATED_BODY()
};

class ICustomizableObjectExtensionNode
{
	GENERATED_BODY()

public:
	/** Generate a mu::Node that produces ExtensionData */
	virtual mu::NodeExtensionDataPtr GenerateMutableNode(FExtensionDataCompilerInterface& CompilerInterface) const = 0;
};
