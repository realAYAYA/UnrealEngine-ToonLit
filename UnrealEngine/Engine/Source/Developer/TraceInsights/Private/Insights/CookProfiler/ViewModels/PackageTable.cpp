// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageTable.h"

// Insights
#include "Insights/CookProfiler/ViewModels/PackageNode.h"
#include "Insights/CookProfiler/ViewModels/PackageTable.h"
#include "Insights/Table/ViewModels/TableCellValueFormatter.h"
#include "Insights/Table/ViewModels/TableCellValueGetter.h"
#include "Insights/Table/ViewModels/TableCellValueSorter.h"
#include "Insights/Table/ViewModels/TableColumn.h"

#define LOCTEXT_NAMESPACE "Insights::FPackageTable"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// Column identifiers

const FName FPackageTableColumns::IdColumnId(TEXT("Id"));
const FName FPackageTableColumns::NameColumnId(TEXT("Name"));
const FName FPackageTableColumns::LoadTimeColumnId(TEXT("LoadTime"));
const FName FPackageTableColumns::SaveTimeColumnId(TEXT("SaveTime"));
const FName FPackageTableColumns::BeginCacheForCookedPlatformDataTimeColumnId(TEXT("BeginCacheForCookedPlatformDataTimeColumnId"));
const FName FPackageTableColumns::GetIsCachedCookedPlatformDataLoadedColumnId(TEXT("GetIsCachedCookedPlatformDataLoaded"));
const FName FPackageTableColumns::PackageAssetClassColumnId(TEXT("AssetClass"));

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef FTableCellValue(*PackageFieldGetter)(const FTableColumn&, const FPackageEntry&);

template<PackageFieldGetter Getter>
class FPackageColumnValueGetter : public FTableCellValueGetter
{
public:
	virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const override
	{
		if (Node.IsGroup())
		{
			const FTableTreeNode& NodePtr = static_cast<const FTableTreeNode&>(Node);
			if (NodePtr.HasAggregatedValue(Column.GetId()))
			{
				return NodePtr.GetAggregatedValue(Column.GetId());
			}
		}
		else //if (Node->Is<FackageNode>())
		{
			const FPackageNode& PackageNode = static_cast<const FPackageNode&>(Node);
			const FPackageEntry* Package = PackageNode.GetPackage();
			if (Package)
			{
				return Getter(Column, *Package);
			}
		}

		return TOptional<FTableCellValue>();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

struct DefaultPackageFieldGetterFuncts
{
	static FTableCellValue GetId(const FTableColumn& Column, const FPackageEntry& Package) { return FTableCellValue((int64)Package.GetId());	}
	static FTableCellValue GetName(const FTableColumn& Column, const FPackageEntry& Package) { return FTableCellValue((const TCHAR*)Package.GetName());	}
	static FTableCellValue GetLoadTime(const FTableColumn& Column, const FPackageEntry& Package) { return FTableCellValue((double)Package.GetLoadTime());	}
	static FTableCellValue GetSaveTime(const FTableColumn& Column, const FPackageEntry& Package) { return FTableCellValue((double)Package.GetSaveTime());	}
	static FTableCellValue GetBeginCacheForCookedPlatformData(const FTableColumn& Column, const FPackageEntry& Package) { return FTableCellValue((double)Package.GetBeginCacheForCookedPlatformData());	}
	static FTableCellValue GetIsCachedCookedPlatformDataLoaded(const FTableColumn& Column, const FPackageEntry& Package) { return FTableCellValue((double)Package.GetIsCachedCookedPlatformDataLoaded());	}
	static FTableCellValue GetAssetClass(const FTableColumn& Column, const FPackageEntry& Package) { return FTableCellValue((const TCHAR*)Package.GetAssetClass());	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTaskTable
////////////////////////////////////////////////////////////////////////////////////////////////////

FPackageTable::FPackageTable()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FPackageTable::~FPackageTable()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FPackageTable::Reset()
{
	FTable::Reset();

	AddDefaultColumns();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FPackageTable::AddDefaultColumns()
{
	//////////////////////////////////////////////////
	// Hierarchy Column
	{
		const int32 HierarchyColumnIndex = -1;
		const TCHAR* HierarchyColumnName = nullptr;
		AddHierarchyColumn(HierarchyColumnIndex, HierarchyColumnName);

		const TSharedRef<FTableColumn>& ColumnRef = GetColumns()[0];
		ColumnRef->SetInitialWidth(200.0f);
		ColumnRef->SetShortName(LOCTEXT("PackageColumnName", "Hierarchy"));
		ColumnRef->SetTitleName(LOCTEXT("PackageColumnTitle", "Package Hierarchy"));
		ColumnRef->SetDescription(LOCTEXT("PackageColumnDesc", "Hierarchy of the package's tree"));
	}

	int32 ColumnIndex = 0;

	//////////////////////////////////////////////////
	// Id Column
	{
		TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FPackageTableColumns::IdColumnId);
		FTableColumn& Column = *ColumnRef;

		Column.SetIndex(ColumnIndex++);

		Column.SetShortName(LOCTEXT("CreatedTimestampColumnName", "Id"));
		Column.SetTitleName(LOCTEXT("CreatedTimestampColumnTitle", "Id"));
		Column.SetDescription(LOCTEXT("CreatedTimestampColumnDesc", "The id of the package."));

		Column.SetFlags(ETableColumnFlags::ShouldBeVisible | ETableColumnFlags::CanBeHidden | ETableColumnFlags::CanBeFiltered);

		Column.SetHorizontalAlignment(HAlign_Left);
		Column.SetInitialWidth(80.0f);

		Column.SetDataType(ETableCellDataType::Int64);

		TSharedRef<ITableCellValueGetter> Getter = MakeShared<FPackageColumnValueGetter<DefaultPackageFieldGetterFuncts::GetId>>();
		Column.SetValueGetter(Getter);

		TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FInt64ValueFormatterAsNumber>();
		Column.SetValueFormatter(Formatter);

		TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByInt64Value>(ColumnRef);
		Column.SetValueSorter(Sorter);

		Column.SetAggregation(ETableColumnAggregation::SameValue);

		AddColumn(ColumnRef);
	}
	//////////////////////////////////////////////////
	// Load Time Column
	{
		TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FPackageTableColumns::LoadTimeColumnId);
		FTableColumn& Column = *ColumnRef;

		Column.SetIndex(ColumnIndex++);

		Column.SetShortName(LOCTEXT("LoadTimeColumnName", "LoadTime"));
		Column.SetTitleName(LOCTEXT("LoadTimeColumnTitle", "LoadTime"));
		Column.SetDescription(LOCTEXT("LoadTimeColumnDesc", "The time it took to load the package."));

		Column.SetFlags(ETableColumnFlags::ShouldBeVisible | ETableColumnFlags::CanBeHidden | ETableColumnFlags::CanBeFiltered);

		Column.SetHorizontalAlignment(HAlign_Left);
		Column.SetInitialWidth(100.0f);

		Column.SetDataType(ETableCellDataType::Double);

		TSharedRef<ITableCellValueGetter> Getter = MakeShared<FPackageColumnValueGetter<DefaultPackageFieldGetterFuncts::GetLoadTime>>();
		Column.SetValueGetter(Getter);

		TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FDoubleValueFormatterAsTimeAuto>();
		Column.SetValueFormatter(Formatter);

		TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByDoubleValue>(ColumnRef);
		Column.SetValueSorter(Sorter);

		Column.SetAggregation(ETableColumnAggregation::Sum);

		AddColumn(ColumnRef);
	}
	//////////////////////////////////////////////////
	// Save Time Column
	{
		TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FPackageTableColumns::SaveTimeColumnId);
		FTableColumn& Column = *ColumnRef;

		Column.SetIndex(ColumnIndex++);

		Column.SetShortName(LOCTEXT("SaveTimeColumnName", "SaveTime"));
		Column.SetTitleName(LOCTEXT("SaveTimeColumnTitle", "SaveTime"));
		Column.SetDescription(LOCTEXT("SaveTimeColumnDesc", "The time it took to save the package."));

		Column.SetFlags(ETableColumnFlags::ShouldBeVisible | ETableColumnFlags::CanBeHidden | ETableColumnFlags::CanBeFiltered);

		Column.SetHorizontalAlignment(HAlign_Left);
		Column.SetInitialWidth(100.0f);

		Column.SetDataType(ETableCellDataType::Double);

		TSharedRef<ITableCellValueGetter> Getter = MakeShared<FPackageColumnValueGetter<DefaultPackageFieldGetterFuncts::GetSaveTime>>();
		Column.SetValueGetter(Getter);

		TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FDoubleValueFormatterAsTimeAuto>();
		Column.SetValueFormatter(Formatter);

		TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByDoubleValue>(ColumnRef);
		Column.SetValueSorter(Sorter);

		Column.SetAggregation(ETableColumnAggregation::Sum);

		AddColumn(ColumnRef);
	}
	//////////////////////////////////////////////////
	// BeginCacheForCookedPlatformData Time Column
	{
		TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FPackageTableColumns::BeginCacheForCookedPlatformDataTimeColumnId);
		FTableColumn& Column = *ColumnRef;

		Column.SetIndex(ColumnIndex++);

		Column.SetShortName(LOCTEXT("BeginCacheForCookedPlatformDataColumnName", "BeginCache"));
		Column.SetTitleName(LOCTEXT("BeginCacheForCookedPlatformDataColumnTitle", "BeginCacheForCookedPlatformData"));
		Column.SetDescription(LOCTEXT("BeginCacheForCookedPlatformDataColumnDesc", "The total time spent in the BeginCacheForCookedPlatformData function for the package."));

		Column.SetFlags(ETableColumnFlags::ShouldBeVisible | ETableColumnFlags::CanBeHidden | ETableColumnFlags::CanBeFiltered);

		Column.SetHorizontalAlignment(HAlign_Left);
		Column.SetInitialWidth(100.0f);

		Column.SetDataType(ETableCellDataType::Double);

		TSharedRef<ITableCellValueGetter> Getter = MakeShared<FPackageColumnValueGetter<DefaultPackageFieldGetterFuncts::GetBeginCacheForCookedPlatformData>>();
		Column.SetValueGetter(Getter);

		TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FDoubleValueFormatterAsTimeAuto>();
		Column.SetValueFormatter(Formatter);

		TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByDoubleValue>(ColumnRef);
		Column.SetValueSorter(Sorter);

		Column.SetAggregation(ETableColumnAggregation::Sum);

		AddColumn(ColumnRef);
	}
	//////////////////////////////////////////////////
	// BeginCacheForCookedPlatformData Time Column
	{
		TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FPackageTableColumns::GetIsCachedCookedPlatformDataLoadedColumnId);
		FTableColumn& Column = *ColumnRef;

		Column.SetIndex(ColumnIndex++);

		Column.SetShortName(LOCTEXT("GetIsCachedCookedPlatformDataLoadedColumnName", "IsCachedCooked"));
		Column.SetTitleName(LOCTEXT("GetIsCachedCookedPlatformDataLoadedColumnTitle", "IsCachedCookedPlatformDataLoaded"));
		Column.SetDescription(LOCTEXT("GetIsCachedCookedPlatformDataLoadedColumnDesc", "The total time spent in the IsCachedCookedPlatformDataLoaded function for the package."));

		Column.SetFlags(ETableColumnFlags::ShouldBeVisible | ETableColumnFlags::CanBeHidden | ETableColumnFlags::CanBeFiltered);

		Column.SetHorizontalAlignment(HAlign_Left);
		Column.SetInitialWidth(100.0f);

		Column.SetDataType(ETableCellDataType::Double);

		TSharedRef<ITableCellValueGetter> Getter = MakeShared<FPackageColumnValueGetter<DefaultPackageFieldGetterFuncts::GetIsCachedCookedPlatformDataLoaded>>();
		Column.SetValueGetter(Getter);

		TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FDoubleValueFormatterAsTimeAuto>();
		Column.SetValueFormatter(Formatter);

		TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByDoubleValue>(ColumnRef);
		Column.SetValueSorter(Sorter);

		Column.SetAggregation(ETableColumnAggregation::Sum);

		AddColumn(ColumnRef);
	}
	//////////////////////////////////////////////////
	// Asset Class Column
	{
		TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FPackageTableColumns::PackageAssetClassColumnId);
		FTableColumn& Column = *ColumnRef;

		Column.SetIndex(ColumnIndex++);

		Column.SetShortName(LOCTEXT("AssetClassColumnName", "Asset Class"));
		Column.SetTitleName(LOCTEXT("AssetClassTitle", "Asset Class"));
		Column.SetDescription(LOCTEXT("AssetClassColumnDesc", "The class of the most significant asset in the package."));

		Column.SetFlags(ETableColumnFlags::ShouldBeVisible | ETableColumnFlags::CanBeHidden | ETableColumnFlags::CanBeFiltered);

		Column.SetHorizontalAlignment(HAlign_Left);
		Column.SetInitialWidth(200.0f);

		Column.SetDataType(ETableCellDataType::CString);

		TSharedRef<ITableCellValueGetter> Getter = MakeShared<FPackageColumnValueGetter<DefaultPackageFieldGetterFuncts::GetAssetClass>>();
		Column.SetValueGetter(Getter);

		TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FCStringValueFormatterAsText>();
		Column.SetValueFormatter(Formatter);

		TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByCStringValue>(ColumnRef);
		Column.SetValueSorter(Sorter);

		Column.SetAggregation(ETableColumnAggregation::SameValue);

		AddColumn(ColumnRef);
	}
	//////////////////////////////////////////////////
	// Package Name Column
	{
		TSharedRef<FTableColumn> ColumnRef = MakeShared<FTableColumn>(FPackageTableColumns::NameColumnId);
		FTableColumn& Column = *ColumnRef;

		Column.SetIndex(ColumnIndex++);

		Column.SetShortName(LOCTEXT("PackageNameColumnName", "Package Name"));
		Column.SetTitleName(LOCTEXT("PackageNameTitle", "Package Name"));
		Column.SetDescription(LOCTEXT("PackageNameColumnDesc", "The name of the package."));

		Column.SetFlags(ETableColumnFlags::CanBeHidden | ETableColumnFlags::CanBeFiltered);

		Column.SetHorizontalAlignment(HAlign_Left);
		Column.SetInitialWidth(400.0f);

		Column.SetDataType(ETableCellDataType::CString);

		TSharedRef<ITableCellValueGetter> Getter = MakeShared<FPackageColumnValueGetter<DefaultPackageFieldGetterFuncts::GetName>>();
		Column.SetValueGetter(Getter);

		TSharedRef<ITableCellValueFormatter> Formatter = MakeShared<FCStringValueFormatterAsText>();
		Column.SetValueFormatter(Formatter);

		TSharedRef<ITableCellValueSorter> Sorter = MakeShared<FSorterByCStringValue>(ColumnRef);
		Column.SetValueSorter(Sorter);

		Column.SetAggregation(ETableColumnAggregation::SameValue);

		AddColumn(ColumnRef);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
