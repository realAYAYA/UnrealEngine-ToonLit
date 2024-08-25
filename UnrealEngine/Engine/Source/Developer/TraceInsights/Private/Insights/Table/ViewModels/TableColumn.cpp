// Copyright Epic Games, Inc. All Rights Reserved.

#include "TableColumn.h"

#include "Insights/Table/ViewModels/TableCellValueFormatter.h"
#include "Insights/Table/ViewModels/TableCellValueGetter.h"
#include "Insights/Table/ViewModels/TableCellValueSorter.h"

#define LOCTEXT_NAMESPACE "Insights::FTableColumn"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableCellValueGetter> FTableColumn::GetDefaultValueGetter()
{
	return MakeShared<FTableCellValueGetter>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TOptional<FTableCellValue> FTableColumn::GetValue(const FBaseTreeNode& InNode) const
{
	return ValueGetter->GetValue(*this, InNode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint64 FTableColumn::GetValueId(const FBaseTreeNode& InNode) const
{
	return ValueGetter->GetValueId(*this, InNode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableCellValueFormatter> FTableColumn::GetDefaultValueFormatter()
{
	return MakeShared<FTableCellValueFormatter>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FTableColumn::GetValueAsText(const FBaseTreeNode& InNode) const
{
	return ValueFormatter->FormatValue(*this, InNode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FTableColumn::GetValueAsTooltipText(const FBaseTreeNode& InNode) const
{
	return ValueFormatter->FormatValueForTooltip(*this, InNode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FTableColumn::GetValueAsGroupingText(const FBaseTreeNode& InNode) const
{
	return ValueFormatter->FormatValueForGrouping(*this, InNode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FTableColumn::CopyValue(const FBaseTreeNode& InNode) const
{
	return ValueFormatter->CopyValue(*this, InNode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FTableColumn::CopyTooltip(const FBaseTreeNode& InNode) const
{
	return ValueFormatter->CopyTooltip(*this, InNode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

inline void TrimEndingZero(FString& Str)
{
	const TCHAR* Start = *Str;
	const TCHAR* End = Start + Str.Len();
	const TCHAR* Data = End;
	while (Data != Start)
	{
		--Data;
		if (*Data != TEXT('0'))
		{
			break;
		}
	}
	if (Data != End)
	{
		if (*Data == TEXT('.'))
		{
			--Data;
		}
		Str.LeftInline(static_cast<int32>(Data - Start + 1));
	}
}

FString FTableColumn::GetValueAsSerializableString(const FBaseTreeNode& InNode) const
{
	const TOptional<FTableCellValue> Value = GetValue(InNode);
	if (!Value.IsSet())
	{
		return FString();
	}
	switch (Value->DataType)
	{
		case ETableCellDataType::Double:
		{
			FString Str = FString::Printf(TEXT("%.9f"), Value->AsDouble());
			TrimEndingZero(Str);
			return Str;
		}
		case ETableCellDataType::Float:
		{
			FString Str = FString::Printf(TEXT("%.6f"), Value->AsFloat());
			TrimEndingZero(Str);
			return Str;
		}
		case ETableCellDataType::Int64:
			return FString::Printf(TEXT("%lli"), Value->AsInt64());
		case ETableCellDataType::Bool:
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		case ETableCellDataType::CString:
			return Value->AsString();
		case ETableCellDataType::Text:
			return Value->AsText().ToString();
		default:
			return GetValueAsText(InNode).ToString();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
