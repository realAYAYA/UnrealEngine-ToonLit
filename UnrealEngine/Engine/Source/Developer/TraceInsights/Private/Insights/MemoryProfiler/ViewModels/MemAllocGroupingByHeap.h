// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/Table/ViewModels/BaseTreeNode.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"
#include "TraceServices/Model/AllocationsProvider.h"

namespace TraceServices
{
	struct FStackFrame;
}

namespace Insights
{

class IAsyncOperationProgress;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemAllocGroupingByHeap : public FTreeNodeGrouping
{
	INSIGHTS_DECLARE_RTTI(FMemAllocGroupingByHeap, FTreeNodeGrouping)

public:
	FMemAllocGroupingByHeap(const TraceServices::IAllocationsProvider& AllocProvider);
	virtual ~FMemAllocGroupingByHeap() override;

	virtual void GroupNodes(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable, IAsyncOperationProgress& InAsyncOperationProgress) const override;
private:
	const TraceServices::IAllocationsProvider& AllocProvider;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
