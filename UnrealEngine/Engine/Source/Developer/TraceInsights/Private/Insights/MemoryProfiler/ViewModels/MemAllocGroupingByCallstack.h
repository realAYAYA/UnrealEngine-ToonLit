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

class FCallstackFrameGroupNode : public FTableTreeNode
{
	INSIGHTS_DECLARE_RTTI(FCallstackFrameGroupNode, FTableTreeNode)

public:
	/** Initialization constructor for the group node. */
	explicit FCallstackFrameGroupNode(const FName InName, TWeakPtr<FTable> InParentTable, const TraceServices::FStackFrame* InStackFrame)
		: FTableTreeNode(InName, InParentTable)
		, StackFrame(InStackFrame)
	{
	}

	virtual ~FCallstackFrameGroupNode()
	{
	}

	virtual const FText GetTooltipText() const override;
	virtual const FSlateBrush* GetIcon() const override;
	virtual FLinearColor GetColor() const override;

	/**
	 * @returns the stack frame.
	 */
	const TraceServices::FStackFrame* GetStackFrame() const
	{
		return StackFrame;
	}

private:
	const TraceServices::FStackFrame* StackFrame = nullptr;
};

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
	FMemAllocGroupingByCallstack(bool bInIsAllocCallstack, bool bInIsInverted, bool bInIsGroupingByFunction);
	virtual ~FMemAllocGroupingByCallstack();

	virtual void GroupNodes(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable, IAsyncOperationProgress& InAsyncOperationProgress) const override;

	bool IsInverted() const { return bIsInverted; }
	bool IsAllocCallstack() const { return bIsAllocCallstack; }

	bool IsGroupingByFunction() const { return bIsGroupingByFunction; }
	void SetGroupingByFunction(bool bOnOff) { bIsGroupingByFunction = bOnOff; }

	bool ShouldSkipFilteredFrames() const { return bShouldSkipFilteredFrames; }
	void SetSkipFilteredFrames(bool bOnOff) { bShouldSkipFilteredFrames = bOnOff; }

private:
	FName GetGroupName(const TraceServices::FStackFrame* Frame) const;

	FCallstackGroup* CreateGroup(TArray<FCallstackGroup*>& InOutAllCallstackGroup, FCallstackGroup* InParentGroup, const FName InGroupName, TWeakPtr<FTable> InParentTable, const TraceServices::FStackFrame* InFrame) const;

	FTableTreeNode* CreateUnsetGroup(TWeakPtr<FTable> ParentTable, FTableTreeNode& Parent) const;
	FTableTreeNode* CreateEmptyCallstackGroup(TWeakPtr<FTable> ParentTable, FTableTreeNode& Parent) const;

private:
	bool bIsAllocCallstack;
	bool bIsInverted;
	std::atomic<bool> bIsGroupingByFunction;
	std::atomic<bool> bShouldSkipFilteredFrames;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
