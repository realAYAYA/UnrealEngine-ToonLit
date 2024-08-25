// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Insights/Common/SimpleRtti.h"
#include "Insights/Table/ViewModels/TableTreeNode.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"
#include "TraceServices/Model/AllocationsProvider.h"
#include "TraceServices/Model/Memory.h"

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

class FMemTagTableTreeNode : public FTableTreeNode
{
	INSIGHTS_DECLARE_RTTI(FMemTagTableTreeNode, FTableTreeNode)

public:
	/** Initialization constructor for a table record node. */
	explicit FMemTagTableTreeNode(const FName InName, TWeakPtr<FTable> InParentTable, int32 InRowIndex, const TCHAR* InTagFullName, bool IsGroup)
		: FTableTreeNode(InName, InParentTable, InRowIndex, IsGroup)
		, TagFullName(InTagFullName)
	{
	}

	/** Initialization constructor for the group node. */
	explicit FMemTagTableTreeNode(const FName InName, TWeakPtr<FTable> InParentTable, const TCHAR* InTagFullName)
		: FTableTreeNode(InName, InParentTable)
		, TagFullName(InTagFullName)
	{
	}

	virtual ~FMemTagTableTreeNode()
	{
	}

	/**
	 * @return the color tint for icon and name text.
	 */
	virtual FLinearColor GetColor() const override
	{
		return FLinearColor(0.75f, 0.5f, 1.0f, 1.0f);
	}

	int64 GetLLMSize() const
	{
		if (LLMSize == INT64_MAX)
		{
			UpdateLLMSize();
		}
		return LLMSize;
	}

private:
	void UpdateLLMSize() const;

private:
	const TCHAR* TagFullName = nullptr;
	mutable int64 LLMSize = INT64_MAX;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
}
