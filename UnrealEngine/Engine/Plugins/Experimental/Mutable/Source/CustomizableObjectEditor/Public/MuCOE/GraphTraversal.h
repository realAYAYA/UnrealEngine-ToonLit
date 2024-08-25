// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

class UCustomizableObject;
class UCustomizableObjectNodeObject;
class UEdGraphPin;
class UCustomizableObjectNode;

/** Follow the given input pin returning the output connected pin.
 *
 * - Skips all orphan pins.
 * - Follows External Pin nodes.
 *
 * @param Pin Pin to follow.
 * @param bOutCycleDetected If provided, it will set to true if a cycle has been found.
 * @return Connected pins. */
TArray<UEdGraphPin*> FollowInputPinArray(const UEdGraphPin& Pin, bool* bOutCycleDetected = nullptr);

/** Non-array version of FollowInputPinArray. The pin can only have one connected pin. */
UEdGraphPin* FollowInputPin(const UEdGraphPin& Pin, bool* bOutCycleDetected = nullptr);

/** Follow the given input output returning the input connected pin.
 * 
 * - Skips all orphan pins.
 * - Follows External Pin nodes.
 * - It will only follow External Pin nodes of loaded CO (i.e., Expose Pin nodes of CO which are NOT loaded will not be found)!
 *
 * @param Pin Pin to follow.
 * @param bOutCycleDetected If provided, it will set to true if a cycle has been found.
 * @return Connected pins. */
TArray<UEdGraphPin*> FollowOutputPinArray(const UEdGraphPin& Pin, bool* bOutCycleDetected = nullptr);

/** Non-array version of FollowOutputPinArray. The pin can only have one connected pin. */
UEdGraphPin* FollowOutputPin(const UEdGraphPin& Pin, bool* CycleDetected = nullptr);

/** Returns the root Object Node of the Customizable Object's graph */
UCustomizableObjectNodeObject* GetRootNode(UCustomizableObject* Object, bool& bOutMultipleBaseObjectsFound);

/** Return in ArrayNodeObject the roots nodes in each Customizable Object graph until the whole root node is found (i.e. the one with parent = nullptr)
 * return false if a cycle is found between Customizable Objects */
bool GetParentsUntilRoot(UCustomizableObject* Object, TArray<UCustomizableObjectNodeObject*>& ArrayNodeObject, TArray<UCustomizableObject*>& ArrayCustomizableObject);

/** Returns true if the Candidate is parent of the current Customizable Object */
bool HasCandidateAsParent(UCustomizableObjectNodeObject* Node, UCustomizableObject* ParentCandidate);

/** Return the full graph Customizable Object root of the given node. */
UCustomizableObject* GetFullGraphRootObject(UCustomizableObjectNodeObject* Node, TArray<UCustomizableObject*>& VisitedObjects);

/** Return Customizable Object of the given node. */
UCustomizableObject* GetRootObject(const UCustomizableObjectNode& Node);

/** Provided a CO object it provides the root CO it is connected. In other words : it returns the root of the entire
	 * mutable graph.
	 * @param InObject Customizable object whose root CO we are asking for.
	 * @return The CO that is the root of the provided Customizable Object. It can be equal to InObject if the provided
	 * object does not have any parent.
*/
CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObject* GetRootObject(UCustomizableObject* ChildObject);

/** Return the full graph Customizable Object Node root of the node given as parameter */
UCustomizableObjectNodeObject* GetFullGraphRootNodeObject(UCustomizableObjectNodeObject* Node, TArray<UCustomizableObject*>& VisitedObjects);

/** Given an output pin, return the output pin where the mesh is located. */
const UEdGraphPin* FindMeshBaseSource(const UEdGraphPin& Pin, const bool bOnlyLookForStaticMesh);

/** Return the mapping of Group Object Nodes to Child Object Nodes of the given hierarchy.
 * @param Object Child or root Object to start the search from. */
TMultiMap<FGuid, UCustomizableObjectNodeObject*> GetNodeGroupObjectNodeMapping(UCustomizableObject* Object);
