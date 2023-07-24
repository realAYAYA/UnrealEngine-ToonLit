// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/ICustomizableObjectInstanceEditor.h"

class UEdGraphNode;

class UCustomizableObject;
class UCustomizableObjectInstance;

// Public interface to Customizable Object Editor
class ICustomizableObjectEditor : public ICustomizableObjectInstanceEditor
{
public:
	// Retrieves the current Customizable Object displayed in the Editor.
	virtual UCustomizableObject* GetCustomizableObject() = 0;
	
	// Checks whether nodes can currently be pasted */
	virtual bool CanPasteNodes() const {return false;}

	// Paste nodes at a specific location
	virtual void PasteNodesHere(const FVector2D& Location) {};

	virtual class UEdGraphNode* CreateCommentBox(const FVector2D& NodePos) { return nullptr; };

	virtual void UpdateGraphNodeProperties() {};

	virtual void UpdateObjectProperties() {};

	// Make sure the graph editor selects a specific node
	virtual void SelectNode(const class UCustomizableObjectNode* Node) {}

	virtual void ReconstructNode(UEdGraphNode* Node) {};
};



#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Toolkits/IToolkitHost.h"
#endif
