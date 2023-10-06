// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/Table/ViewModels/BaseTreeNode.h"
#include "Insights/Table/ViewModels/TableCellValue.h"

namespace Insights
{

class FTable;
class STableTreeView;

struct FTableRowId
{
	static constexpr int32 InvalidRowIndex = -1;

	FTableRowId(int32 InRowIndex) : RowIndex(InRowIndex) {}

	bool HasValidIndex() const { return RowIndex >= 0; }

	int32 RowIndex;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTableTreeNode;

/** Type definition for shared pointers to instances of FTableTreeNode. */
typedef TSharedPtr<class FTableTreeNode> FTableTreeNodePtr;

/** Type definition for shared references to instances of FTableTreeNode. */
typedef TSharedRef<class FTableTreeNode> FTableTreeNodeRef;

/** Type definition for shared references to const instances of FTableTreeNode. */
typedef TSharedRef<const class FTableTreeNode> FTableTreeNodeRefConst;

/** Type definition for weak references to instances of FTableTreeNode. */
typedef TWeakPtr<class FTableTreeNode> FTableTreeNodeWeak;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Table Tree Node View Model.
 * Class used to store information about a generic table tree node (used in STableTreeView).
 */
class FTableTreeNode : public FBaseTreeNode
{
	INSIGHTS_DECLARE_RTTI(FTableTreeNode, FBaseTreeNode)

public:
	/** Initialization constructor for a table record node. */
	explicit FTableTreeNode(const FName InName, TWeakPtr<FTable> InParentTable, int32 InRowIndex)
		: FBaseTreeNode(InName, false)
		, ParentTable(InParentTable)
		, RowId(InRowIndex)
		, AggregatedValues(nullptr)
	{
	}

	/** Initialization constructor for a group node. */
	explicit FTableTreeNode(const FName InGroupName, TWeakPtr<FTable> InParentTable)
		: FBaseTreeNode(InGroupName, true)
		, ParentTable(InParentTable)
		, RowId(FTableRowId::InvalidRowIndex)
		, AggregatedValues(nullptr)
	{
	}

	/** Initialization constructor for a table record node. */
	explicit FTableTreeNode(const FName InName, TWeakPtr<FTable> InParentTable, int32 InRowIndex, bool IsGroup)
		: FBaseTreeNode(InName, IsGroup)
		, ParentTable(InParentTable)
		, RowId(InRowIndex)
		, AggregatedValues(nullptr)
	{
	}

	virtual ~FTableTreeNode()
	{
		CleanupAggregatedValues();
	}

	const TWeakPtr<FTable>& GetParentTable() const { return ParentTable; }
	FTableRowId GetRowId() const { return RowId; }
	int32 GetRowIndex() const { return RowId.RowIndex; }

	//////////////////////////////////////////////////
	// Aggregation

	void InitAggregatedValues()
	{
		if (!AggregatedValues)
		{
			AggregatedValues = new TMap<FName, FTableCellValue>();
		}
	}

	void CleanupAggregatedValues()
	{
		if (AggregatedValues)
		{
			delete AggregatedValues;
			AggregatedValues = nullptr;
		}
	}

	void ResetAggregatedValues()
	{
		CleanupAggregatedValues();
	}

	void ResetAggregatedValue(const FName& ColumnId)
	{
		if (AggregatedValues)
		{
			AggregatedValues->Remove(ColumnId);
		}
	}

	bool HasAggregatedValue(const FName& ColumnId) const
	{
		return AggregatedValues && AggregatedValues->Contains(ColumnId);
	}

	const FTableCellValue* FindAggregatedValue(const FName& ColumnId) const
	{
		return AggregatedValues ? AggregatedValues->Find(ColumnId) : nullptr;
	}

	const FTableCellValue& GetAggregatedValue(const FName& ColumnId) const
	{
		return AggregatedValues->FindChecked(ColumnId);
	}

	void SetAggregatedValue(const FName& ColumnId, const FTableCellValue& Value)
	{
		InitAggregatedValues();
		AggregatedValues->Add(ColumnId, Value);
	}

	//////////////////////////////////////////////////

	virtual bool IsFiltered() const { return bIsFiltered; }
	virtual void SetIsFiltered(bool InValue) { bIsFiltered = InValue; }

	virtual bool OnLazyCreateChildren(TSharedPtr<STableTreeView> InTableTreeView) { return false; }

protected:
	TWeakPtr<FTable> ParentTable;
	FTableRowId RowId;

	TMap<FName, FTableCellValue>* AggregatedValues;

private:
	bool bIsFiltered = false;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FCustomTableTreeNode : public FTableTreeNode
{
	INSIGHTS_DECLARE_RTTI(FCustomTableTreeNode, FTableTreeNode)

public:
	/** Initialization constructor for a table record node. */
	explicit FCustomTableTreeNode(const FName InName, TWeakPtr<FTable> InParentTable, int32 InRowIndex, const FSlateBrush* InIconBrush, FLinearColor InColor, bool IsGroup)
		: FTableTreeNode(InName, InParentTable, InRowIndex, IsGroup)
		, IconBrush(InIconBrush)
		, Color(InColor)
	{
	}

	/** Initialization constructor for the group node. */
	explicit FCustomTableTreeNode(const FName InName, TWeakPtr<FTable> InParentTable, const FSlateBrush* InIconBrush, FLinearColor InColor)
		: FTableTreeNode(InName, InParentTable)
		, IconBrush(InIconBrush)
		, Color(InColor)
	{
	}

	virtual ~FCustomTableTreeNode()
	{
	}

	/**
	 * @return a brush icon for this node.
	 */
	virtual const FSlateBrush* GetIcon() const override
	{
		return IconBrush;
	}

	/**
	 * Sets an icon brush for this node.
	 */
	void SetIcon(const FSlateBrush* InIconBrush)
	{
		IconBrush = InIconBrush;
	}

	/**
	 * @return the color tint for icon and name text.
	 */
	virtual FLinearColor GetColor() const override
	{
		return Color;
	}

private:
	/** The icon of this node. */
	const FSlateBrush* IconBrush;

	/** The color tint of this node. */
	FLinearColor Color;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
