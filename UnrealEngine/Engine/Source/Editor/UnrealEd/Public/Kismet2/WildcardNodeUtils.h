// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/NameTypes.h"

/**
* A collection of utilities for creating, using, and changing the value of wildcard pin types
* throughout the graph editor.
*/
class FWildcardNodeUtils
{
public:
	
	/** 
	* Gets the default wildcard pin type. Useful for doing comparisons on other pin types 
	* and checks during compilation of nodes.
	*/
	static UNREALED_API FEdGraphPinType GetDefaultWildcardPinType();

	/**
	* Checks if the given pin is in a wildcard state
	*
	* @param	Pin		The pin the consider
	* @return	True if the given pin is a Wildcard pin
	*/
	static UNREALED_API bool IsWildcardPin(const UEdGraphPin* const Pin);

	/**
	* Checks if the given pin is linked to any wildcard pins
	* 
	* @return	True if the given pin is linked to any wildcard pins
	*/
	static UNREALED_API bool IsLinkedToWildcard(const UEdGraphPin* const Pin);

	/**
	* Add a default wildcard pin to the given node
	* 
	* @param Node				The node to add this pin to
	* @param PinName			Name of the given wildcard pin
	* @param Direction			
	* @param ContainerType		
	* @return	The newly created pin or nullptr if failed
	*/
	static UNREALED_API UEdGraphPin* CreateWildcardPin(UEdGraphNode* Node, const FName PinName, const EEdGraphPinDirection Direction, const EPinContainerType ContainerType = EPinContainerType::None);

	/**
	* Check this node for any wildcard pins
	*
	* @return	True if the given node has any wildcard pins on it
	*/
	static UNREALED_API bool NodeHasAnyWildcards(const UEdGraphNode* const Node);
};
