// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Insights/Common/SimpleRtti.h"
#include "Insights/Table/ViewModels/TableTreeNode.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"
#include "TraceServices/Model/AllocationsProvider.h"

namespace Insights
{

class InAsyncOperationProgress;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemAllocGroupingByTag : public FTreeNodeGrouping
{
	INSIGHTS_DECLARE_RTTI(FMemAllocGroupingByTag, FTreeNodeGrouping);

public:
	FMemAllocGroupingByTag(const TraceServices::IAllocationsProvider& TagProvider);

	virtual void GroupNodes(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable, IAsyncOperationProgress& InAsyncOperationProgress) const override;

private:
	const TraceServices::IAllocationsProvider& TagProvider;
};
	
////////////////////////////////////////////////////////////////////////////////////////////////////
}
