// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDetailTableRowBase.h"

#include "PropertyHandleImpl.h"

const float SDetailTableRowBase::ScrollBarPadding = 16.0f;

int32 SDetailTableRowBase::GetIndentLevelForBackgroundColor() const
{
	int32 IndentLevel = 0; 
	if (OwnerTablePtr.IsValid())
	{
		// every item is in a category, but we don't want to show an indent for "top-level" properties
		IndentLevel = GetIndentLevel() - 1;
	}

	TSharedPtr<FDetailTreeNode> DetailTreeNode = OwnerTreeNode.Pin();
	if (DetailTreeNode.IsValid() && 
		DetailTreeNode->GetDetailsView() != nullptr && 
		DetailTreeNode->GetDetailsView()->ContainsMultipleTopLevelObjects())
	{
		// if the row is in a multiple top level object display (eg. Project Settings), don't display an indent for the initial level
		--IndentLevel;
	}

	return FMath::Max(0, IndentLevel);
}

bool SDetailTableRowBase::IsScrollBarVisible(TWeakPtr<STableViewBase> OwnerTableViewWeak)
{
	TSharedPtr<STableViewBase> OwnerTableView = OwnerTableViewWeak.Pin();
	if (OwnerTableView.IsValid())
	{
		return OwnerTableView->GetScrollbarVisibility() == EVisibility::Visible;
	}
	return false;
}

TArray<TSharedPtr<FPropertyNode>> SDetailTableRowBase::GetPropertyNodes(const bool& bRecursive) const
{
	TArray<TSharedPtr<IPropertyHandle>> PropertyHandles = GetPropertyHandles(bRecursive);

	TArray<TSharedPtr<FPropertyNode>> PropertyNodes;
	PropertyNodes.Reserve(PropertyHandles.Num());
	
	for (const TSharedPtr<IPropertyHandle>& PropertyHandle : PropertyHandles)
	{
		if (PropertyHandle->IsValidHandle())
		{
			PropertyNodes.Add(StaticCastSharedPtr<FPropertyHandleBase>(PropertyHandle)->GetPropertyNode());
		}
	}

	return PropertyNodes;
}

TArray<TSharedPtr<IPropertyHandle>> SDetailTableRowBase::GetPropertyHandles(const bool& bRecursive) const
{
	TFunction<bool(const TSharedPtr<IDetailTreeNode>&, TArray<TSharedPtr<IPropertyHandle>>&)> AppendPropertyHandles;
	AppendPropertyHandles = [&AppendPropertyHandles, bRecursive]
		(const TSharedPtr<IDetailTreeNode>& InParent, TArray<TSharedPtr<IPropertyHandle>>& OutPropertyHandles)
	{
		// Parent in first call is actually parent of this row, so these are the nodes for this row 
		TArray<TSharedRef<IDetailTreeNode>> ChildNodes;
		InParent->GetChildren(ChildNodes, true);

		if (ChildNodes.IsEmpty() || !bRecursive)
		{
			return false;
		}

		OutPropertyHandles.Reserve(OutPropertyHandles.Num() + ChildNodes.Num());		
		for (const TSharedRef<IDetailTreeNode>& ChildNode : ChildNodes)
		{
			// @fixme: this won't return when there's multiple properties in a single row, or the row is custom 
			TSharedPtr<IPropertyHandle> ChildPropertyHandle = ChildNode->CreatePropertyHandle();
			if (ChildPropertyHandle.IsValid() && ChildPropertyHandle->IsValidHandle())
			{
				OutPropertyHandles.Add(ChildPropertyHandle);
			}
			AppendPropertyHandles(ChildNode, OutPropertyHandles);
		}

		return true;
	};

	TArray<TSharedPtr<IPropertyHandle>> PropertyHandles;	
	if (const TSharedPtr<FDetailTreeNode> OwnerTreeNodePtr = OwnerTreeNode.Pin())
	{
		AppendPropertyHandles(OwnerTreeNodePtr, PropertyHandles);
	}

	PropertyHandles.Remove(nullptr);

	return PropertyHandles;
}
