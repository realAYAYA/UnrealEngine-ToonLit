// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Insights/Common/SimpleRtti.h"

namespace Insights
{

class ITableCellValueSorter;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FBaseTreeNode;

/** Type definition for shared pointers to instances of FBaseTreeNode. */
typedef TSharedPtr<class FBaseTreeNode> FBaseTreeNodePtr;

/** Type definition for shared references to instances of FBaseTreeNode. */
typedef TSharedRef<class FBaseTreeNode> FBaseTreeNodeRef;

/** Type definition for shared references to const instances of FBaseTreeNode. */
typedef TSharedRef<const class FBaseTreeNode> FBaseTreeNodeRefConst;

/** Type definition for weak references to instances of FBaseTreeNode. */
typedef TWeakPtr<class FBaseTreeNode> FBaseTreeNodeWeak;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FBaseTreeNode : public TSharedFromThis<FBaseTreeNode>
{
	INSIGHTS_DECLARE_RTTI_BASE(FBaseTreeNode)

protected:
	struct FGroupNodeData
	{
		/** Children of the group node. */
		TArray<FBaseTreeNodePtr> Children;

		/** Filtered children of the group node. */
		TArray<FBaseTreeNodePtr> FilteredChildren;

		/** Descriptive text for the group node to display in a tooltip. */
		FText Tooltip;

		/** A generic pointer to a data context for the group node. Can be nullptr. */
		void* Context = nullptr;

		/** Whether the group node should be expanded or not. */
		bool bIsExpanded = false;
	};

public:
	/** Initialization constructor for the node. */
	FBaseTreeNode(const FName InName, bool bInIsGroup)
		: DefaultSortOrder(0)
		, Name(InName)
		, GroupData(bInIsGroup ? new FGroupNodeData() : &DefaultGroupData)
	{
	}

	virtual ~FBaseTreeNode()
	{
		if (GroupData != &DefaultGroupData)
		{
			delete GroupData;
			GroupData = nullptr;
		}
	}

	uint32 GetDefaultSortOrder() const
	{
		return DefaultSortOrder;
	}
	void SetDefaultSortOrder(uint32 Order)
	{
		DefaultSortOrder = Order;
	}

	/**
	 * @return a name of this node.
	 */
	const FName& GetName() const
	{
		return Name;
	}

	/**
	 * @return a name of this node to display in tree view.
	 */
	virtual const FText GetDisplayName() const;

	/**
	 * @return a name suffix for this node to display in tree view. Ex.: group nodes may include additional info, like the number of visible / total children.
	 */
	virtual const FText GetExtraDisplayName() const;

	virtual bool HasExtraDisplayName() const;

	/**
	 * @return a descriptive text for this node to display in a tooltip.
	 */
	virtual const FText GetTooltip() const
	{
		return GroupData->Tooltip;
	}

	/**
	 * Sets a descriptive text for this node to display in a tooltip.
	 */
	virtual void SetTooltip(const FText& InTooltip)
	{
		if (IsGroup())
		{
			GroupData->Tooltip = InTooltip;
		}
	}

	/**
	 * @return a pointer to a data context for this node.
	 */
	virtual void* GetContext() const
	{
		return GroupData->Context;
	}

	/**
	 * Sets a pointer to a data context for this node.
	 */
	virtual void SetContext(void* InContext)
	{
		if (IsGroup())
		{
			GroupData->Context = InContext;
		}
	}

	/**
	 * @return true, if this node is a group node.
	 */
	bool IsGroup() const
	{
		return GroupData != &DefaultGroupData;
	}

	/**
	 * Initializes the group data, allowing this node to accept children.
	 */
	void InitGroupData()
	{
		if (GroupData == &DefaultGroupData)
		{
			GroupData = new FGroupNodeData();
		}
	}

	/**
	 * @return a const reference to the child nodes of this group.
	 */
	FORCEINLINE_DEBUGGABLE const TArray<FBaseTreeNodePtr>& GetChildren() const
	{
		return GroupData->Children;
	}

	/**
	 * @return a const reference to the child nodes that should be visible to the UI based on filtering.
	 */
	FORCEINLINE_DEBUGGABLE const TArray<FBaseTreeNodePtr>& GetFilteredChildren() const
	{
		return GroupData->FilteredChildren;
	}

	/**
	 * @return a weak reference to the group of this node, may be invalid.
	 */
	FBaseTreeNodeWeak GetGroupPtr() const
	{
		return GroupPtr;
	}

	/**
	 * @return a name of the group node that this node belongs to.
	 */
	const FName& GetGroupName() const
	{
		return GroupPtr.Pin()->GetName();
	}

	virtual bool IsFiltered() const
	{
		return false;
	}

	void SortChildrenAscending(const ITableCellValueSorter& Sorter);
	void SortChildrenDescending(const ITableCellValueSorter& Sorter);

	template <class PREDICATE_CLASS>
	void SortChildren(const PREDICATE_CLASS& Predicate)
	{
		GroupData->Children.Sort(Predicate);
	}

	/** Adds specified child to the children and sets group for it. */
	FORCEINLINE_DEBUGGABLE void AddChildAndSetGroupPtr(const FBaseTreeNodePtr& ChildPtr)
	{
		ChildPtr->GroupPtr = AsShared();
		GroupData->Children.Add(ChildPtr);
	}

	void SetGroupPtrForAllChildren()
	{
		FBaseTreeNodeWeak ThisNode = AsShared();
		for (FBaseTreeNodePtr& NodePtr : GroupData->Children)
		{
			NodePtr->GroupPtr = ThisNode;
		}
	}

	void ResetGroupPtrForAllChildren()
	{
		for (FBaseTreeNodePtr& NodePtr : GroupData->Children)
		{
			NodePtr->GroupPtr = nullptr;
		}
	}

	/** Clears children. */
	FORCEINLINE_DEBUGGABLE void ClearChildren(int32 NewSize = 0)
	{
		ResetGroupPtrForAllChildren();
		GroupData->Children.Reset(NewSize);
	}

	FORCEINLINE_DEBUGGABLE void SwapChildren(TArray<FBaseTreeNodePtr>& NewChildren)
	{
		ResetGroupPtrForAllChildren();
		Swap(NewChildren, GroupData->Children);
		SetGroupPtrForAllChildren();
	}

	FORCEINLINE_DEBUGGABLE void SwapChildrenFast(TArray<FBaseTreeNodePtr>& NewChildren)
	{
		Swap(NewChildren, GroupData->Children);
	}

	/** Adds specified child to the filtered children. */
	FORCEINLINE_DEBUGGABLE void AddFilteredChild(const FBaseTreeNodePtr& ChildPtr)
	{
		GroupData->FilteredChildren.Add(ChildPtr);
	}

	/** Clears filtered children. */
	void ClearFilteredChildren(int32 NewSize = 0)
	{
		GroupData->FilteredChildren.Reset(NewSize);
	}

	bool IsExpanded() const
	{
		return GroupData->bIsExpanded;
	}
	void SetExpansion(bool bOnOff)
	{
		if (IsGroup())
		{
			GroupData->bIsExpanded = bOnOff;
		}
	}

protected:
	/**
	 * @return a reference to the child nodes of this group.
	 */
	FORCEINLINE_DEBUGGABLE TArray<FBaseTreeNodePtr>& GetChildrenMutable()
	{
		return GroupData->Children;
	}

	void SetGroupPtr(FBaseTreeNodePtr InGroupPtr)
	{
		GroupPtr = InGroupPtr;
	}

private:
	/** The default sort order. Index used to optimize sorting. */
	int32 DefaultSortOrder;

	/** The name of this node. */
	const FName Name;

	/** A weak pointer to the group/parent of this node. */
	FBaseTreeNodeWeak GroupPtr;

	/** The struct containing properties of a group node. It is allocated only for group nodes. */
	FGroupNodeData* GroupData;

	/** The only group data for "not a group" nodes. */
	static FGroupNodeData DefaultGroupData;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
