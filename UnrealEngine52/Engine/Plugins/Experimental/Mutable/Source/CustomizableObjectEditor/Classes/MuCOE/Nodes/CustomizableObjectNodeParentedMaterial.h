// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MuCOE/Nodes/CustomizableObjectNodeParentedNode.h"

class UCustomizableObjectNode;
class UCustomizableObjectNodeMaterial;


/** Node Material specialization of ICustomizableObjectNodeParentedNode. */
class FCustomizableObjectNodeParentedMaterial : public ICustomizableObjectNodeParentedNode
{
public:
	// Own interface
	/** Return the parent material node. */
	UCustomizableObjectNodeMaterial* GetParentMaterialNode() const;
	
	/** Return all possible parent material nodes of the node. */
	TArray<UCustomizableObjectNodeMaterial*> GetPossibleParentMaterialNodes() const;

	/** Returns the parent material node if there exist a path to it. */
	UCustomizableObjectNodeMaterial* GetParentMaterialNodeIfPath() const;
	
protected:
	/** Return the node which this interface belongs to. */
	virtual UCustomizableObjectNode& GetNode() = 0;
	const UCustomizableObjectNode& GetNode() const;
};

