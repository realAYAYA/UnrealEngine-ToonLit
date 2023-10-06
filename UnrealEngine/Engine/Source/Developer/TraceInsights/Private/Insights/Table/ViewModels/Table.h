// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Insights
{

class FTableColumn;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Table View Model.
 * View model class for the STableListView and STableTreeView widgets.
 */
class FTable : public TSharedFromThis<FTable>
{
public:
	FTable();
	virtual ~FTable();

	const FText& GetDisplayName() const { return DisplayName; }
	void SetDisplayName(const FText& InDisplayName) { DisplayName = InDisplayName; }

	const FText& GetDescription() const { return Description; }
	void SetDescription(const FText& InDescription) { Description = InDescription; }

	virtual void Reset();

	int32 GetColumnCount() const { return Columns.Num(); }
	bool IsValid() const { return Columns.Num() > 0; }

	const TArray<TSharedRef<FTableColumn>>& GetColumns() const { return Columns; }
	void SetColumns(const TArray<TSharedRef<FTableColumn>>& InColumns);

	void GetVisibleColumns(TArray<TSharedRef<FTableColumn>>& InArray) const;

	TSharedRef<FTableColumn> FindColumnChecked(const FName& ColumnId) const
	{
		return ColumnIdToPtrMapping.FindChecked(ColumnId);
	}

	TSharedPtr<FTableColumn> FindColumn(const FName& ColumnId) const
	{
		const TSharedRef<FTableColumn>* const ColumnRefPtr = ColumnIdToPtrMapping.Find(ColumnId);
		if (ColumnRefPtr != nullptr)
		{
			return *ColumnRefPtr;
		}
		return nullptr;
	}

	int32 GetColumnPositionIndex(const FName& ColumnId) const;

	void GetVisibleColumnsData(const TArray<TSharedPtr<class FBaseTreeNode>>& InNodes, const FName& LogListingName, TCHAR Separator, bool bIncludeHeaders, FString& OutData) const;

	static const FName GetHierarchyColumnId();

protected:
	void ResetColumns() { Columns.Reset(); }
	void AddColumn(TSharedRef<FTableColumn> Column);
	void AddHierarchyColumn(int32 ColumnIndex, const TCHAR* ColumnName);

private:
	FText DisplayName;
	FText Description;

	/** All available columns. */
	TArray<TSharedRef<FTableColumn>> Columns;

	/** Mapping between column Ids and FTableColumn shared refs. */
	TMap<FName, TSharedRef<FTableColumn>> ColumnIdToPtrMapping;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
