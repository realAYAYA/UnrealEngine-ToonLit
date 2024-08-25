// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseTreeNode.h"

#include "Styling/SlateBrush.h"
#include "Styling/StyleColors.h"

// Insights
#include "Insights/Common/InsightsStyle.h"
#include "Insights/Table/ViewModels/TableCellValueSorter.h"

#define LOCTEXT_NAMESPACE "UE::Insights::FBaseTreeNode"

namespace UE
{
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
		const int32 NumChildren = GetChildrenCount();
		const int32 NumFilteredChildren = GetFilteredChildrenCount();

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

const FSlateBrush* FBaseTreeNode::GetDefaultIcon(bool bIsGroupNode)
{
	if (bIsGroupNode)
	{
		return FInsightsStyle::GetBrush("Icons.Group.TreeItem");
	}
	else
	{
		return FInsightsStyle::GetBrush("Icons.Leaf.TreeItem");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FLinearColor FBaseTreeNode::GetDefaultColor(bool bIsGroupNode)
{
	if (bIsGroupNode)
	{
		return USlateThemeManager::Get().GetColor(EStyleColor::AccentYellow);
	}
	else
	{
		return USlateThemeManager::Get().GetColor(EStyleColor::AccentWhite);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FBaseTreeNode::SortChildren(const ITableCellValueSorter& Sorter, ESortMode SortMode)
{
	Sorter.Sort(GroupData->Children, SortMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FBaseTreeNode::SortFilteredChildren(const ITableCellValueSorter& Sorter, ESortMode SortMode)
{
	Sorter.Sort(*GroupData->FilteredChildrenPtr, SortMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
} // namespace UE

#undef LOCTEXT_NAMESPACE
