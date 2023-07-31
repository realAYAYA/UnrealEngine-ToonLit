// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemTagTreeViewColumnFactory.h"

// Insights
#include "Insights/Table/ViewModels/TableCellValueFormatter.h"
#include "Insights/Table/ViewModels/TableCellValueGetter.h"
#include "Insights/Table/ViewModels/TableCellValueSorter.h"
#include "Insights/MemoryProfiler/ViewModels/MemTagNodeGroupingAndSorting.h"
#include "Insights/MemoryProfiler/ViewModels/MemTagNodeHelper.h"

#define LOCTEXT_NAMESPACE "SMemTagTreeView"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Column identifiers

const FName FMemTagTreeViewColumns::NameColumnID(TEXT("Name")); // TEXT("_Hierarchy")
const FName FMemTagTreeViewColumns::TypeColumnID(TEXT("Type"));
const FName FMemTagTreeViewColumns::TrackerColumnID(TEXT("Tracker"));
const FName FMemTagTreeViewColumns::InstanceCountColumnID(TEXT("Count"));
const FName FMemTagTreeViewColumns::MinValueColumnID(TEXT("Min"));
const FName FMemTagTreeViewColumns::MaxValueColumnID(TEXT("Max"));
const FName FMemTagTreeViewColumns::AverageValueColumnID(TEXT("Average"));

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemTagTreeViewColumnFactory::CreateMemTagTreeViewColumns(TArray<TSharedRef<Insights::FTableColumn>>& Columns)
{
	Columns.Reset();

	Columns.Add(CreateNameColumn());
	Columns.Add(CreateTypeColumn());
	Columns.Add(CreateTrackerColumn());
	//Columns.Add(CreateInstanceCountColumn());
	//Columns.Add(CreateMinValueColumn());
	//Columns.Add(CreateMaxValueColumn());
	//Columns.Add(CreateAverageValueColumn());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<Insights::FTableColumn> FMemTagTreeViewColumnFactory::CreateNameColumn()
{
	using namespace Insights;

	TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FMemTagTreeViewColumns::NameColumnID);
	FTableColumn& Column = *ColumnRef;

	Column.SetShortName(LOCTEXT("Name_ColumnName", "Name"));
	Column.SetTitleName(LOCTEXT("Name_ColumnTitle", "LLM Tag or Group Name"));
	Column.SetDescription(LOCTEXT("Name_ColumnDesc", "Name of the LLM tag or group"));

	Column.SetFlags(ETableColumnFlags::ShouldBeVisible |
					ETableColumnFlags::CanBeFiltered |
					ETableColumnFlags::IsHierarchy);

	Column.SetHorizontalAlignment(HAlign_Left);
	Column.SetInitialWidth(206.0f);
	Column.SetMinWidth(42.0f);

	Column.SetDataType(ETableCellDataType::Text);

	TSharedRef<ITableCellValueGetter> Getter = MakeShared<FDisplayNameValueGetter>();
	Column.SetValueGetter(Getter);

	TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FTextValueFormatter>();
	Column.SetValueFormatter(Formatter);

	TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByName>(ColumnRef);
	Column.SetValueSorter(Sorter);

	return ColumnRef;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<Insights::FTableColumn> FMemTagTreeViewColumnFactory::CreateTypeColumn()
{
	using namespace Insights;

	TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FMemTagTreeViewColumns::TypeColumnID);
	FTableColumn& Column = *ColumnRef;

	Column.SetShortName(LOCTEXT("Type_ColumnName", "Type"));
	Column.SetTitleName(LOCTEXT("Type_ColumnTitle", "Type"));
	Column.SetDescription(LOCTEXT("Type_ColumnDesc", "Type of LLM tag or group"));

	Column.SetFlags(ETableColumnFlags::CanBeHidden |
					//ETableColumnFlags::ShouldBeVisible |
					ETableColumnFlags::CanBeFiltered);

	Column.SetHorizontalAlignment(HAlign_Left);
	Column.SetInitialWidth(60.0f);

	Column.SetDataType(ETableCellDataType::Text);

	class FMemTagNodeTypeValueGetter : public FTableCellValueGetter
	{
	public:
		virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
		{
			ensure(Column.GetId() == FMemTagTreeViewColumns::TypeColumnID);
			const FMemTagNode& MemTagNode = static_cast<const FMemTagNode&>(Node);
			return FTableCellValue(MemTagNodeTypeHelper::ToText(MemTagNode.GetType()));
		}
	};

	TSharedRef<ITableCellValueGetter> Getter = MakeShared<FMemTagNodeTypeValueGetter>();
	Column.SetValueGetter(Getter);

	TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FTextValueFormatter>();
	Column.SetValueFormatter(Formatter);

	//TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByTextValue>(ColumnRef);
	TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FMemTagNodeSortingByType>(ColumnRef);
	Column.SetValueSorter(Sorter);

	return ColumnRef;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<Insights::FTableColumn> FMemTagTreeViewColumnFactory::CreateTrackerColumn()
{
	using namespace Insights;

	TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FMemTagTreeViewColumns::TrackerColumnID);
	FTableColumn& Column = *ColumnRef;

	Column.SetShortName(LOCTEXT("Tracker_ColumnName", "Tracker"));
	Column.SetTitleName(LOCTEXT("Tracker_ColumnTitle", "Tracker"));
	Column.SetDescription(LOCTEXT("Tracker_ColumnDesc", "Tracker using the LLM tag"));

	Column.SetFlags(ETableColumnFlags::CanBeHidden |
					ETableColumnFlags::ShouldBeVisible |
					ETableColumnFlags::CanBeFiltered);

	Column.SetHorizontalAlignment(HAlign_Left);
	Column.SetInitialWidth(60.0f);

	Column.SetDataType(ETableCellDataType::Text);

	class FMemTagNodeTypeValueGetter : public FTableCellValueGetter
	{
	public:
		virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
		{
			ensure(Column.GetId() == FMemTagTreeViewColumns::TrackerColumnID);
			const FMemTagNode& MemTagNode = static_cast<const FMemTagNode&>(Node);
			return FTableCellValue(MemTagNode.GetTrackerText());
		}
	};

	TSharedRef<ITableCellValueGetter> Getter = MakeShared<FMemTagNodeTypeValueGetter>();
	Column.SetValueGetter(Getter);

	TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FTextValueFormatter>();
	Column.SetValueFormatter(Formatter);

	TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FMemTagNodeSortingByTracker>(ColumnRef);
	Column.SetValueSorter(Sorter);

	return ColumnRef;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<Insights::FTableColumn> FMemTagTreeViewColumnFactory::CreateInstanceCountColumn()
{
	using namespace Insights;

	TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FMemTagTreeViewColumns::InstanceCountColumnID);
	FTableColumn& Column = *ColumnRef;

	Column.SetShortName(LOCTEXT("InstanceCount_ColumnName", "Count"));
	Column.SetTitleName(LOCTEXT("InstanceCount_ColumnTitle", "Instance Count"));
	Column.SetDescription(LOCTEXT("InstanceCount_ColumnDesc", "Number of LLM tag instances"));

	Column.SetFlags(ETableColumnFlags::CanBeHidden |
					//ETableColumnFlags::ShouldBeVisible |
					ETableColumnFlags::CanBeFiltered);

	Column.SetHorizontalAlignment(HAlign_Right);
	Column.SetInitialWidth(60.0f);

	Column.SetDataType(ETableCellDataType::Int64);

	class FInstanceCountValueGetter : public FTableCellValueGetter
	{
	public:
		virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
		{
			ensure(Column.GetId() == FMemTagTreeViewColumns::InstanceCountColumnID);
			const FMemTagNode& MemTagNode = static_cast<const FMemTagNode&>(Node);
			return FTableCellValue(static_cast<int64>(MemTagNode.GetAggregatedStats().InstanceCount));
		}
	};

	TSharedRef<ITableCellValueGetter> Getter = MakeShared<FInstanceCountValueGetter>();
	Column.SetValueGetter(Getter);

	TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FInt64ValueFormatterAsNumber>();
	Column.SetValueFormatter(Formatter);

	//TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByInt64Value>(ColumnRef);
	TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FMemTagNodeSortingByInstanceCount>(ColumnRef);
	Column.SetValueSorter(Sorter);

	return ColumnRef;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<Insights::FTableColumn> FMemTagTreeViewColumnFactory::CreateMinValueColumn()
{
	using namespace Insights;

	TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FMemTagTreeViewColumns::MinValueColumnID);
	FTableColumn& Column = *ColumnRef;

	Column.SetShortName(LOCTEXT("MinValue_ColumnName", "I.Min"));
	Column.SetTitleName(LOCTEXT("MinValue_ColumnTitle", "Min"));
	Column.SetDescription(LOCTEXT("MinValue_ColumnDesc", "Minimum value (bytes) of selected LLM tag instances"));

	Column.SetFlags(ETableColumnFlags::CanBeHidden |
		//ETableColumnFlags::ShouldBeVisible |
		ETableColumnFlags::CanBeFiltered);

	Column.SetHorizontalAlignment(HAlign_Right);
	Column.SetInitialWidth(ValueColumnInitialWidth);

	Column.SetDataType(ETableCellDataType::Int64);

	class FMaxValueValueGetter : public FTableCellValueGetter
	{
	public:
		virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
		{
			ensure(Column.GetId() == FMemTagTreeViewColumns::MinValueColumnID);
			const FMemTagNode& MemTagNode = static_cast<const FMemTagNode&>(Node);
			return FTableCellValue(static_cast<int64>(MemTagNode.GetAggregatedStats().Min));
		}
	};

	TSharedRef<ITableCellValueGetter> Getter = MakeShared<FMaxValueValueGetter>();
	Column.SetValueGetter(Getter);

	TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FInt64ValueFormatterAsNumber>();
	Column.SetValueFormatter(Formatter);

	TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByInt64Value>(ColumnRef);
	Column.SetValueSorter(Sorter);

	return ColumnRef;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<Insights::FTableColumn> FMemTagTreeViewColumnFactory::CreateMaxValueColumn()
{
	using namespace Insights;

	TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FMemTagTreeViewColumns::MaxValueColumnID);
	FTableColumn& Column = *ColumnRef;

	Column.SetShortName(LOCTEXT("MaxValue_ColumnName", "I.Max"));
	Column.SetTitleName(LOCTEXT("MaxValue_ColumnTitle", "Max"));
	Column.SetDescription(LOCTEXT("MaxValue_ColumnDesc", "Maximum value (bytes) of selected LLM tag instances"));

	Column.SetFlags(ETableColumnFlags::CanBeHidden |
					//ETableColumnFlags::ShouldBeVisible |
					ETableColumnFlags::CanBeFiltered);

	Column.SetHorizontalAlignment(HAlign_Right);
	Column.SetInitialWidth(ValueColumnInitialWidth);

	Column.SetDataType(ETableCellDataType::Int64);

	class FMaxValueValueGetter : public FTableCellValueGetter
	{
	public:
		virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
		{
			ensure(Column.GetId() == FMemTagTreeViewColumns::MaxValueColumnID);
			const FMemTagNode& MemTagNode = static_cast<const FMemTagNode&>(Node);
			return FTableCellValue(static_cast<int64>(MemTagNode.GetAggregatedStats().Max));
		}
	};

	TSharedRef<ITableCellValueGetter> Getter = MakeShared<FMaxValueValueGetter>();
	Column.SetValueGetter(Getter);

	TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FInt64ValueFormatterAsNumber>();
	Column.SetValueFormatter(Formatter);

	TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByInt64Value>(ColumnRef);
	Column.SetValueSorter(Sorter);

	return ColumnRef;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<Insights::FTableColumn> FMemTagTreeViewColumnFactory::CreateAverageValueColumn()
{
	using namespace Insights;

	TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FMemTagTreeViewColumns::AverageValueColumnID);
	FTableColumn& Column = *ColumnRef;

	Column.SetShortName(LOCTEXT("AvgValue_ColumnName", "I.Avg"));
	Column.SetTitleName(LOCTEXT("AvgValue_ColumnTitle", "Average"));
	Column.SetDescription(LOCTEXT("AvgValue_ColumnDesc", "Average value (bytes) of selected LLM tag instances"));

	Column.SetFlags(ETableColumnFlags::CanBeHidden |
					//ETableColumnFlags::ShouldBeVisible |
					ETableColumnFlags::CanBeFiltered);

	Column.SetHorizontalAlignment(HAlign_Right);
	Column.SetInitialWidth(ValueColumnInitialWidth);

	Column.SetDataType(ETableCellDataType::Int64);

	class FAverageValueValueGetter : public FTableCellValueGetter
	{
	public:
		virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
		{
			ensure(Column.GetId() == FMemTagTreeViewColumns::AverageValueColumnID);
			const FMemTagNode& MemTagNode = static_cast<const FMemTagNode&>(Node);
			return FTableCellValue(static_cast<int64>(MemTagNode.GetAggregatedStats().Average));
		}
	};

	TSharedRef<ITableCellValueGetter> Getter = MakeShared<FAverageValueValueGetter>();
	Column.SetValueGetter(Getter);

	TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FInt64ValueFormatterAsNumber>();
	Column.SetValueFormatter(Formatter);

	TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByInt64Value>(ColumnRef);
	Column.SetValueSorter(Sorter);

	return ColumnRef;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
