// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Layout/Visibility.h"

#include "Insights/Table/ViewModels/TableCellValue.h"

class IToolTip;

namespace Insights
{

class FBaseTreeNode;
class FTableColumn;

////////////////////////////////////////////////////////////////////////////////////////////////////

class ITableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const = 0;
	virtual FText FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const = 0;

	virtual FText FormatValue(const FTableColumn& Column, const FBaseTreeNode& Node) const = 0;
	virtual FText FormatValueForTooltip(const FTableColumn& Column, const FBaseTreeNode& Node) const = 0;
	virtual FText FormatValueForGrouping(const FTableColumn& Column, const FBaseTreeNode& Node) const = 0;

	virtual FText CopyValue(const FTableColumn& Column, const FBaseTreeNode& Node) const = 0;
	virtual FText CopyTooltip(const FTableColumn& Column, const FBaseTreeNode& Node) const = 0;

	virtual TSharedPtr<IToolTip> GetCustomTooltip(const FTableColumn& Column, const FBaseTreeNode& Node) const = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTableCellValueFormatter : public ITableCellValueFormatter
{
public:
	FTableCellValueFormatter() {}
	virtual ~FTableCellValueFormatter() {}

	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override { return FText::GetEmpty(); }
	virtual FText FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const override { return FormatValue(InValue); }

	virtual FText FormatValue(const FTableColumn& Column, const FBaseTreeNode& Node) const override; // { return FormatValue(Column.GetValue(Node)); }
	virtual FText FormatValueForTooltip(const FTableColumn& Column, const FBaseTreeNode& Node) const override; // { return FormatValueForTooltip(Column.GetValue(Node)); }
	virtual FText FormatValueForGrouping(const FTableColumn& Column, const FBaseTreeNode& Node) const override; // { return FormatValueForTooltip(Column.GetValue(Node)); }

	virtual FText CopyValue(const FTableColumn& Column, const FBaseTreeNode& Node) const override; // { return FormatValue(Column, Node); }
	virtual FText CopyTooltip(const FTableColumn& Column, const FBaseTreeNode& Node) const override; // { return FormatValueForTooltip(Column, Node); }

	virtual TSharedPtr<IToolTip> GetCustomTooltip(const FTableColumn& Column, const FBaseTreeNode& Node) const override;

	static EVisibility GetTooltipVisibility();
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTextValueFormatter : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override
	{
		if (InValue.IsSet())
		{
			return InValue.GetValue().GetText();
		}
		return FText::GetEmpty();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FAsTextValueFormatter : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override
	{
		if (InValue.IsSet())
		{
			return InValue.GetValue().AsText();
		}
		return FText::GetEmpty();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FBoolValueFormatterAsTrueFalse : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FBoolValueFormatterAsOnOff : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FInt64ValueFormatterAsNumber : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override
	{
		return InValue.IsSet() ? FText::AsNumber(InValue.GetValue().Int64) : FText::GetEmpty();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FInt64ValueFormatterAsUInt32InfinteNumber : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
	virtual FText FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FInt64ValueFormatterAsHex32 : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override
	{
		return InValue.IsSet() ? FText::FromString(FString::Printf(TEXT("0x%08X"), static_cast<uint32>(InValue.GetValue().Int64))) : FText::GetEmpty();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FInt64ValueFormatterAsHex64 : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override
	{
		return InValue.IsSet() ? FText::FromString(FString::Printf(TEXT("0x%016llX"), static_cast<uint64>(InValue.GetValue().Int64))) : FText::GetEmpty();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FInt64ValueFormatterAsMemory : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
	virtual FText FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFloatValueFormatterAsNumber : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFloatValueFormatterAsTimeAuto : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
	virtual FText FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FDoubleValueFormatterAsNumber : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FDoubleValueFormatterAsTimeAuto : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
	virtual FText FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FDoubleValueFormatterAsTimeMs : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
	virtual FText FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FCStringValueFormatterAsText : public FTableCellValueFormatter
{
public:
	virtual FText FormatValue(const TOptional<FTableCellValue>& InValue) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
