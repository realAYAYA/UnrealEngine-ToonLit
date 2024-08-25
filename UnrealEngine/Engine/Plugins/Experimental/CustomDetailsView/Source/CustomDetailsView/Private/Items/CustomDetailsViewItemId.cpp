// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/CustomDetailsViewItemId.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

FCustomDetailsViewItemId FCustomDetailsViewItemId::MakeCategoryId(FName InCategoryName)
{
	FCustomDetailsViewItemId OutViewItemId;
	OutViewItemId.ItemName = InCategoryName.ToString();
	OutViewItemId.ItemType = static_cast<uint32>(EDetailNodeType::Category);
	OutViewItemId.CalculateTypeHash();
	return OutViewItemId;
}

FCustomDetailsViewItemId FCustomDetailsViewItemId::MakePropertyId(FProperty* Property)
{
	FCustomDetailsViewItemId OutViewItemId;
	if (Property)
	{
		OutViewItemId.ItemName = Property->GetFullName();
		OutViewItemId.ItemType = static_cast<uint32>(EDetailNodeType::Item);
		OutViewItemId.CalculateTypeHash();
	}
	return OutViewItemId;
}

FCustomDetailsViewItemId FCustomDetailsViewItemId::MakePropertyId(const TSharedPtr<IPropertyHandle>& InPropertyHandle)
{
	if (InPropertyHandle.IsValid())
	{
		return FCustomDetailsViewItemId::MakePropertyId(InPropertyHandle->GetProperty());	
	}
	return FCustomDetailsViewItemId();
}

FCustomDetailsViewItemId FCustomDetailsViewItemId::MakePropertyId(FName InItemName)
{
	FCustomDetailsViewItemId OutViewItemId;
	OutViewItemId.ItemName = InItemName.ToString();
	// Object is the last in the enum. Add 1 for custom entry.
	OutViewItemId.ItemType = CustomItemType;
	OutViewItemId.CalculateTypeHash();

	return OutViewItemId;
}

FCustomDetailsViewItemId FCustomDetailsViewItemId::MakeFromDetailTreeNode(const TSharedRef<IDetailTreeNode>& InDetailTreeNode)
{
	if (const TSharedPtr<IPropertyHandle> PropertyHandle = InDetailTreeNode->CreatePropertyHandle())
	{
		return FCustomDetailsViewItemId::MakePropertyId(PropertyHandle);
	}

	if (InDetailTreeNode->GetNodeType() == EDetailNodeType::Category)
	{
		return FCustomDetailsViewItemId::MakeCategoryId(InDetailTreeNode->GetNodeName());
	}

	FCustomDetailsViewItemId OutViewItemId;
	OutViewItemId.ItemName = InDetailTreeNode->GetNodeName().ToString();
	OutViewItemId.ItemType = static_cast<uint32>(InDetailTreeNode->GetNodeType());
	OutViewItemId.CalculateTypeHash();
	return OutViewItemId;
}

bool FCustomDetailsViewItemId::IsType(EDetailNodeType InType) const
{
	return ItemType == static_cast<uint32>(InType);
}
