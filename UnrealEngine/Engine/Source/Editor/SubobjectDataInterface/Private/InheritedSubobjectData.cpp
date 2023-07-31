// Copyright Epic Games, Inc. All Rights Reserved.

#include "InheritedSubobjectData.h"

#include "ComponentInstanceDataCache.h"
#include "Components/ActorComponent.h"
#include "Kismet2/ComponentEditorUtils.h"

class USCS_Node;

FInheritedSubobjectData::FInheritedSubobjectData(UObject* ContextObject, const FSubobjectDataHandle& ParentHandle, const bool InbIsInheritedSCS)
    : FSubobjectData(ContextObject, ParentHandle)
	, bIsInheritedSCS(InbIsInheritedSCS)
{
}

bool FInheritedSubobjectData::IsNativeComponent() const
{
	if (const UActorComponent* Template = GetComponentTemplate())
    {
    	return Template->CreationMethod == EComponentCreationMethod::Native && GetSCSNode() == nullptr;
    }
	
	return false;
}

bool FInheritedSubobjectData::CanEdit() const
{
	if(IsComponent())
	{
		if(IsInstancedInheritedComponent())
		{
			const UActorComponent* Template = GetComponentTemplate();
			return (Template ? Template->IsEditableWhenInherited() : false);
		}
		else if (!IsNativeComponent())
		{
			USCS_Node* SCS_Node = GetSCSNode();
			return (SCS_Node != nullptr);
		}
		else if (const UActorComponent* ComponentTemplate = GetComponentTemplate())
		{
			return FComponentEditorUtils::GetPropertyForEditableNativeComponent(ComponentTemplate) != nullptr;
		}
	}
	
	return FSubobjectData::CanEdit();
}

bool FInheritedSubobjectData::CanDelete() const
{
	if(IsInheritedComponent() || (IsDefaultSceneRoot() && SceneRootHasDefaultName()) || (GetSCSNode() != nullptr && IsInstancedInheritedComponent()) || IsChildActorSubtreeObject())
	{
		return false;
	}
	
	return true;
}

bool FInheritedSubobjectData::IsInheritedSCSNode() const
{
	return bIsInheritedSCS;
}
