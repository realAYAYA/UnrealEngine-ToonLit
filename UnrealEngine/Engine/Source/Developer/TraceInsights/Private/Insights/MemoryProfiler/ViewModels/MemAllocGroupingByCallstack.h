// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/Table/ViewModels/BaseTreeNode.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"

namespace TraceServices
{
	struct FStackFrame;
}

namespace Insights
{

class IAsyncOperationProgress;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemAllocGroupingByCallstack : public FTreeNodeGrouping
{
	INSIGHTS_DECLARE_RTTI(FMemAllocGroupingByCallstack, FTreeNodeGrouping)

private:
	struct FCallstackGroup
	{
		FCallstackGroup* Parent = nullptr;
		FName Name;
		FTableTreeNode* Node = nullptr;
		const TraceServices::FStackFrame* Frame = nullptr;
		TMap<uint64, FCallstackGroup*> GroupMap; // Callstack Frame Address -> FCallstackGroup*
		TMap<FName, FCallstackGroup*> GroupMapByName; // Group Name --> FCallstackGroup*
	};

public:
	FMemAllocGroupingByCallstack(bool bInIsInverted, bool bInIsGroupingByFunction);
	virtual ~FMemAllocGroupingByCallstack();

	virtual void GroupNodes(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable, IAsyncOperationProgress& InAsyncOperationProgress) const override;

	bool IsInverted() const { return bIsInverted; }

	bool IsGroupingByFunction() const { return bIsGroupingByFunction; }
	void SetGroupingByFunction(bool bOnOff) { bIsGroupingByFunction = bOnOff; }

private:
	FName GetGroupName(const TraceServices::FStackFrame* Frame) const;
	FText GetGroupTooltip(const TraceServices::FStackFrame* Frame) const;

	FCallstackGroup* CreateGroup(TArray<FCallstackGroup*>& InOutAllCallstackGroup, FCallstackGroup* InParentGroup, const FName InGroupName, TWeakPtr<FTable> InParentTable, const TraceServices::FStackFrame* InFrame) const;

	FTableTreeNode* CreateUnsetGroup(TWeakPtr<FTable> ParentTable, FTableTreeNode& Parent) const;
	FTableTreeNode* CreateEmptyCallstackGroup(TWeakPtr<FTable> ParentTable, FTableTreeNode& Parent) const;

private:
	bool bIsInverted;
	bool bIsGroupingByFunction;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
