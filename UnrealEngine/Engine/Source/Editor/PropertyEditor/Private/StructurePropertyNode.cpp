// Copyright Epic Games, Inc. All Rights Reserved.


#include "StructurePropertyNode.h"
#include "ItemPropertyNode.h"
#include "PropertyEditorHelpers.h"

void FStructurePropertyNode::InitChildNodes()
{
	const bool bShouldShowHiddenProperties = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
	const bool bShouldShowDisableEditOnInstance = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowDisableEditOnInstance);

	const UStruct* Struct = StructData.IsValid() ? StructData->GetStruct() : nullptr;

	TArray<FProperty*> StructMembers;

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* StructMember = *It;
		if (PropertyEditorHelpers::ShouldBeVisible(*this, StructMember))
		{
			StructMembers.Add(StructMember);
		}
	}

	PropertyEditorHelpers::OrderPropertiesFromMetadata(StructMembers);

	for (FProperty* StructMember : StructMembers)
	{
		TSharedPtr<FItemPropertyNode> NewItemNode(new FItemPropertyNode);//;//CreatePropertyItem(StructMember,INDEX_NONE,this);

		FPropertyNodeInitParams InitParams;
		InitParams.ParentNode = SharedThis(this);
		InitParams.Property = StructMember;
		InitParams.ArrayOffset = 0;
		InitParams.ArrayIndex = INDEX_NONE;
		InitParams.bAllowChildren = true;
		InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
		InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;
		InitParams.bCreateCategoryNodes = false;

		NewItemNode->InitNode(InitParams);
		AddChildNode(NewItemNode);
	}
}
