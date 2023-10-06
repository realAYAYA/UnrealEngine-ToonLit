// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/ICustomizableObjectInstanceEditor.h"

class UEdGraphNode;

class UCustomizableObject;
class UCustomizableObjectInstance;
class UClass;
class UCustomizableObjectNode;


// Public interface to Customizable Object Editor
class ICustomizableObjectEditor : public ICustomizableObjectInstanceEditor
{
public:
	// Retrieves the current Customizable Object displayed in the Editor.
	virtual UCustomizableObject* GetCustomizableObject() = 0;
	
	// Checks whether nodes can currently be pasted */
	virtual bool CanPasteNodes() const {return false;}

	// Paste nodes at a specific location
	virtual void PasteNodesHere(const FVector2D& Location) {}

	virtual UEdGraphNode* CreateCommentBox(const FVector2D& NodePos) { return nullptr; }

	virtual void UpdateGraphNodeProperties() {}

	virtual void UpdateObjectProperties() {}

	// Make sure the graph editor selects a specific node
	virtual void SelectNode(const UCustomizableObjectNode* Node) {}

	virtual void ReconstructNode(UEdGraphNode* Node) {}

	/** Reconstructs all child the nodes that match the given type.
	 * @param StartNode Root node to start the graph traversal. This one also will be reconstructed.
	 * @param NodeType Node types to reconstruct. */
	virtual void ReconstructAllChildNodes(UCustomizableObjectNode& StartNode, const UClass& NodeType) {}
};



#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Toolkits/IToolkitHost.h"
#endif
