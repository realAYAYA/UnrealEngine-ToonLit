// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/Table/ViewModels/BaseTreeNode.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"

namespace Insights
{

class IAsyncOperationProgress;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemAllocGroupingBySize : public FTreeNodeGrouping
{
	INSIGHTS_DECLARE_RTTI(FMemAllocGroupingBySize, FTreeNodeGrouping)

public:
	struct FThreshold
	{
		uint64 Size; // inclusive upper limit
		FText Name; // group name
	};

public:
	FMemAllocGroupingBySize();
	virtual ~FMemAllocGroupingBySize();

	const TArray<FThreshold>& GetThresholds() const { return Thresholds; }
	TArray<FThreshold>& EditThresholds() { bIsPow2 = false; return Thresholds; }

	bool IsPow2() const { return bIsPow2; }
	void ResetThresholdsPow2();

	virtual void GroupNodes(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable, IAsyncOperationProgress& InAsyncOperationProgress) const override;

private:
	TArray<FThreshold> Thresholds;
	bool bIsPow2; // thresholds are automatically set as power of two
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
