// Copyright Epic Games, Inc. All Rights Reserved.

#include "TableCellValueGetter.h"

#include "Insights/Table/ViewModels/BaseTreeNode.h"

#define LOCTEXT_NAMESPACE "Insights::FTableCellValueGetter"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

const TOptional<FTableCellValue> FNameValueGetter::GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
{
	return TOptional<FTableCellValue>(FTableCellValue(FText::FromName(Node.GetName())));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TOptional<FTableCellValue> FDisplayNameValueGetter::GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
{
	return TOptional<FTableCellValue>(FTableCellValue(Node.GetDisplayName()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
