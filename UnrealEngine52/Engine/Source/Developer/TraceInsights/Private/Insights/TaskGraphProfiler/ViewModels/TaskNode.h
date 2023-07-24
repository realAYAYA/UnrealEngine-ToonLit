// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/TaskGraphProfiler/ViewModels/TaskTable.h"
#include "Insights/TaskGraphProfiler/ViewModels/TaskEntry.h"
#include "Insights/Table/ViewModels/TableTreeNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{

enum class ETaskNodeType
{
	/** The TaskNode is an allocation node. */
	Task,

	/** The TaskNode is a group node. */
	Group,

	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTaskNode;

/** Type definition for shared pointers to instances of FTaskNode. */
typedef TSharedPtr<class FTaskNode> FTaskNodePtr;

/** Type definition for shared references to instances of FTaskNode. */
typedef TSharedRef<class FTaskNode> FTaskNodeRef;

/** Type definition for shared references to const instances of FTaskNode. */
typedef TSharedRef<const class FTaskNode> FTaskNodeRefConst;

/** Type definition for weak references to instances of FTaskNode. */
typedef TWeakPtr<class FTaskNode> FTaskNodeWeak;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Class used to store information about a task node (used in the STaskTreeView).
 */
class FTaskNode : public FTableTreeNode
{
	INSIGHTS_DECLARE_RTTI(FTaskNode, FTableTreeNode)

public:
	/** Initialization constructor for the Task node. */
	explicit FTaskNode(const FName InName, TWeakPtr<FTaskTable> InParentTable, int32 InRowIndex)
		: FTableTreeNode(InName, InParentTable, InRowIndex)
		, Type(ETaskNodeType::Task)
	{
	}

	/** Initialization constructor for the group node. */
	explicit FTaskNode(const FName InGroupName, TWeakPtr<FTaskTable> InParentTable)
		: FTableTreeNode(InGroupName, InParentTable)
		, Type(ETaskNodeType::Group)
	{
	}

	/**
	 * @return a type of this Task node or ETaskNodeType::Group for group nodes.
	 */
	ETaskNodeType GetType() const { return Type; }

	FTaskTable& GetTaskTableChecked() const
	{
		const TSharedPtr<FTable>& TablePin = GetParentTable().Pin();
		check(TablePin.IsValid());
		return *StaticCastSharedPtr<FTaskTable>(TablePin);
	}

	bool IsValidTask() const { return GetTaskTableChecked().IsValidRowIndex(GetRowIndex()); }
	const FTaskEntry* GetTask() const { return GetTaskTableChecked().GetTask(GetRowIndex()); }
	const FTaskEntry& GetTaskChecked() const { return GetTaskTableChecked().GetTaskChecked(GetRowIndex()); }

private:
	const ETaskNodeType Type;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
