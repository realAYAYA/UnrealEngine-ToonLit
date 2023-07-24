// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hierarchies/ModifierHierarchyRules.h"

FName UModifierHierarchyRules::GetRootNode_Implementation() const
{
	unimplemented();
	return {};
}

bool UModifierHierarchyRules::GetParentNode_Implementation(FName ChildNode, FName& ParentNode) const
{
	return false;
}

TSet<FName> UModifierHierarchyRules::GetChildNodes_Implementation(FName Node) const
{
	unimplemented();
	return {};
}

UVCamModifier* UModifierHierarchyRules::GetModifierInNode_Implementation(UVCamComponent* Component, FName NodeName) const
{
	unimplemented();
	return {};
}

bool UModifierHierarchyRules::GetConnectionPointTargetForNode_Implementation(FName GroupName, UVCamComponent* Component, FVCamModifierConnectionBinding& Connection) const
{
	unimplemented();
	return false;
}

TSet<FName> UModifierHierarchyRules::GetNodesContainingModifier_Implementation(UVCamModifier* Modifier) const
{
	unimplemented();
	return {};
}
