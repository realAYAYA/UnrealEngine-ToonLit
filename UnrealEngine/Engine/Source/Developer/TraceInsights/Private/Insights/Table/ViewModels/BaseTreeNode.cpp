// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseTreeNode.h"

// Insights
#include "Insights/Table/ViewModels/TableCellValueSorter.h"

#define LOCTEXT_NAMESPACE "Insights::FBaseTreeNode"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FBaseTreeNode)

FBaseTreeNode::FGroupNodeData FBaseTreeNode::DefaultGroupData;

////////////////////////////////////////////////////////////////////////////////////////////////////

const FText FBaseTreeNode::GetDisplayName() const
{
	return FText::FromName(GetName());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FText FBaseTreeNode::GetExtraDisplayName() const
{
	if (IsGroup())
	{
		const int32 NumChildren = GroupData->Children.Num();
		const int32 NumFilteredChildren = GroupData->FilteredChildren.Num();

		if (NumFilteredChildren == NumChildren)
		{
			return FText::Format(LOCTEXT("TreeNodeGroup_ExtraText_Fmt1", "({0})"), FText::AsNumber(NumChildren));
		}
		else
		{
			return FText::Format(LOCTEXT("TreeNodeGroup_ExtraText_Fmt2", "({0} / {1})"), FText::AsNumber(NumFilteredChildren), FText::AsNumber(NumChildren));
		}
	}

	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FBaseTreeNode::HasExtraDisplayName() const
{
	return IsGroup();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FBaseTreeNode::SortChildrenAscending(const ITableCellValueSorter& Sorter)
{
	Sorter.Sort(GroupData->Children, ESortMode::Ascending);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FBaseTreeNode::SortChildrenDescending(const ITableCellValueSorter& Sorter)
{
	Sorter.Sort(GroupData->Children, ESortMode::Descending);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
