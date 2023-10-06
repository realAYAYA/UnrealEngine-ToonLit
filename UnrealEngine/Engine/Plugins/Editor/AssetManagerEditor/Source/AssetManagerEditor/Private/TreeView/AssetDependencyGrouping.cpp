// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDependencyGrouping.h"
#include "AssetTreeNode.h"
#include "AssetTable.h"
#include "Insights/Common/AsyncOperationProgress.h"

#define LOCTEXT_NAMESPACE "FAssetDependencyGrouping"

INSIGHTS_IMPLEMENT_RTTI(FAssetDependencyGrouping)
INSIGHTS_IMPLEMENT_RTTI(FPluginDependencyGrouping)

////////////////////////////////////////////////////////////////////////////////////////////////////
// FAssetDependencyGrouping
////////////////////////////////////////////////////////////////////////////////////////////////////

FAssetDependencyGrouping::FAssetDependencyGrouping()
	: FTreeNodeGrouping(
		LOCTEXT("Grouping_ByDependency_ShortName", "Dependency"),
		LOCTEXT("Grouping_ByDependency_TitleName", "By Dependency"),
		LOCTEXT("Grouping_ByDependency_Desc", "Group assets based on their dependency."),
		TEXT("Icons.Group.TreeItem"),
		nullptr)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAssetDependencyGrouping::~FAssetDependencyGrouping()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAssetDependencyGrouping::GroupNodes(const TArray<UE::Insights::FTableTreeNodePtr>& Nodes, UE::Insights::FTableTreeNode& ParentGroup, TWeakPtr<UE::Insights::FTable> InParentTable, UE::Insights::IAsyncOperationProgress& InAsyncOperationProgress) const
{
	using namespace UE::Insights;

	ParentGroup.ClearChildren();

	TSharedPtr<FAssetTable> AssetTable = StaticCastSharedPtr<FAssetTable>(InParentTable.Pin());
	check(AssetTable.IsValid());

	for (FTableTreeNodePtr NodePtr : Nodes)
	{
		if (InAsyncOperationProgress.ShouldCancelAsyncOp())
		{
			return;
		}

		if (NodePtr->IsGroup() || !NodePtr->Is<FAssetTreeNode>())
		{
			ParentGroup.AddChildAndSetParent(NodePtr);
			continue;
		}

		// For each (visible) asset, we will create the following hierarchy:

		// If the asset does not have dependencies, the asset node is added directly (no group node is created).
		// |
		// +-- [asset:{AssetName}]

		// By default, the "Dependencies" node is collapsed.
		// |
		// +-- [group:{AssetName}] (self + dependencies)
		//     |
		//     +-- [group:_Self]
		//     |   |
		//     |   +-- [asset:{AssetName}]
		//     |
		//     +-- [group:Dependencies] (double click to expand)

		// When the "Dependencies" node is expanded, it will be populated with actual dependencies.
		// |
		// +-- [group:{AssetName}] (self + dependencies)
		//     |
		//     +-- [group:_Self]
		//     |   |
		//     |   +-- [asset:{AssetName}]
		//     |
		//     +-- [group:Dependencies]
		//         |
		//         +-- [asset:{DependentAsset1}]
		//         |
		//         +-- [asset:{DependentAsset2}]
		//         |
		//         +-- [group:{DependentAsset3}] (self + dependencies)
		//         |   |
		//         |   ...
		//         ...

		FAssetTreeNode& AssetNode = NodePtr->As<FAssetTreeNode>();
		const FAssetTableRow& Asset = AssetNode.GetAssetChecked();

		if (Asset.GetNumDependencies() > 0)
		{
			// Create a group for the asset node (self) + dependencies.
			FTableTreeNodePtr AssetGroupPtr = MakeShared<FAssetTreeNode>(Asset.GetNodeName(), AssetNode.GetAssetTableWeak(), AssetNode.GetRowIndex(), true);
			AssetGroupPtr->SetExpansion(false);
			ParentGroup.AddChildAndSetParent(AssetGroupPtr);

			// Add the asset node (self) under a "_self" group node.
			static FName SelfGroupName(TEXT("_Self_")); // used _ prefix to sort before "Dependencies"
			FTableTreeNodePtr SelfGroupPtr = MakeShared<FAssetTreeNode>(SelfGroupName, AssetNode.GetAssetTableWeak(), AssetNode.GetRowIndex(), true);
			SelfGroupPtr->SetExpansion(false);
			SelfGroupPtr->AddChildAndSetParent(NodePtr);
			AssetGroupPtr->AddChildAndSetParent(SelfGroupPtr);

			// Create a group node for all dependent assets of the current asset.
			// The actual nodes for the dependent assets are lazy created by this group node.
			static FName DependenciesGroupName(TEXT("Dependencies"));
			TSharedPtr<FAssetDependenciesGroupTreeNode> DependenciesGroupPtr = MakeShared<FAssetDependenciesGroupTreeNode>(DependenciesGroupName, AssetTable, AssetNode.GetRowIndex());
			DependenciesGroupPtr->SetExpansion(false);
			AssetGroupPtr->AddChildAndSetParent(DependenciesGroupPtr);
		}
		else
		{
			// No dependencies. Just add the asset node (self).
			ParentGroup.AddChildAndSetParent(NodePtr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FPluginDependencyGrouping
////////////////////////////////////////////////////////////////////////////////////////////////////

FPluginDependencyGrouping::FPluginDependencyGrouping()
	: FTreeNodeGrouping(
		LOCTEXT("Grouping_ByPluginDependency_ShortName", "Plugin Dependency"),
		LOCTEXT("Grouping_ByPluginDependency_TitleName", "By Plugin Dependency"),
		LOCTEXT("Grouping_ByPluginDependency_Desc", "Group assets based on their plugin. Each plugin group node will all show the dependent plugins."),
		TEXT("Icons.Group.TreeItem"),
		nullptr)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FPluginDependencyGrouping::~FPluginDependencyGrouping()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FPluginDependencyGrouping::GroupNodes(const TArray<UE::Insights::FTableTreeNodePtr>& Nodes, UE::Insights::FTableTreeNode& ParentGroup, TWeakPtr<UE::Insights::FTable> InParentTable, UE::Insights::IAsyncOperationProgress& InAsyncOperationProgress) const
{
	using namespace UE::Insights;

	ParentGroup.ClearChildren();

	TSharedPtr<FAssetTable> AssetTable = StaticCastSharedPtr<FAssetTable>(InParentTable.Pin());
	check(AssetTable.IsValid());

	TMap<int32, FPluginSimpleGroupNode*> PluginIndexToGroupNodeMap;

	for (FTableTreeNodePtr NodePtr : Nodes)
	{
		if (InAsyncOperationProgress.ShouldCancelAsyncOp())
		{
			return;
		}

		if (NodePtr->IsGroup() || !NodePtr->Is<FAssetTreeNode>())
		{
			ParentGroup.AddChildAndSetParent(NodePtr);
			continue;
		}

		// For each (visible) asset, we will create the following hierarchy:

		// If the plugin does not have dependencies, a simple group will be created.
		// |
		// +-- [group:{PluginName}] (self) // FPluginSimpleGroupNode
		// |   |
		// |   +-- [asset:{AssetName1}]
		// |   |
		// |   +-- [asset:{AssetName2}]
		// |   |
		// |   ...

		// By default, the "Plugin Dependencies" node is collapsed.
		// |
		// +-- [group:{PluginName}] (self + dependencies) // FPluginAndDependenciesGroupNode
		// |   |
		// |   +-- [group:Plugin Dependencies] (double click to expand) // FPluginDependenciesGroupNode, lazy
		// |   |
		// |   +-- [group:{PluginName}] (self) // FPluginSimpleGroupNode
		// |       |
		// |       +-- [asset:{AssetName1}]
		// |       |
		// |       +-- [asset:{AssetName2}]
		// |       |
		// |       ...

		// When the "Plugin Dependencies" node is expanded, it will be populated with actual plugin dependencies.
		// |
		// +-- [group:{PluginName}] (self + dependencies) // FPluginAndDependenciesGroupNode
		// |   |
		// |   +-- [group:Plugin Dependencies] // FPluginDependenciesGroupNode expanded
		// |   |   |
		// |   |   +-- [group:{DependentPlugin1}] (self + dependencies) // FPluginAndDependenciesGroupNode
		// |   |   |   |
		// |   |   |   +-- [group:Plugin Dependencies] (double click to expand) // FPluginDependenciesGroupNode, lazy
		// |   |   |   |
		// |   |   |   +-- [group:{DependentPlugin1}] (self) // FPluginSimpleGroupNode
		// |   |   |       |
		// |   |   |       +-- [asset:{Asset1a}]
		// |   |   |       |
		// |   |   |       +-- [asset:{Asset1b}]
		// |   |   |       ...
		// |   |   |
		// |   |   +-- [group:{DependentPlugin2}] (self, no further dependencies) // FPluginSimpleGroupNode
		// |   |   |   |
		// |   |   |   +-- [asset:{Asset2a}]
		// |   |   |   |
		// |   |   |   +-- [asset:{Asset2b}]
		// |   |   |   ...
		// |   |   |
		// |   |   ...
		// |   |
		// |   +-- [group:{PluginName}] (self) // FPluginSimpleGroupNode
		// |       |
		// |       +-- [asset:{Asset1a}]
		// |       |
		// |       +-- [asset:{Asset1b}]
		// |       |
		// |       ...

		FAssetTreeNode& AssetNode = NodePtr->As<FAssetTreeNode>();
		const FAssetTableRow& Asset = AssetNode.GetAssetChecked();

		const TCHAR* PluginName = Asset.GetPluginName();
		const FAssetTablePluginInfo& PluginInfo = AssetTable->GetOrCreatePluginInfo(PluginName);
		int32 PluginIndex = AssetTable->GetIndexForPlugin(PluginName);

		FPluginSimpleGroupNode* PluginGroup = PluginIndexToGroupNodeMap.FindRef(PluginIndex);
		if (PluginGroup)
		{
			// Plugin group node (simple or with dependencies) was already created. Just add the current asset to it.
			PluginGroup->AddChildAndSetParent(NodePtr);
		}
		else if (PluginInfo.PluginDependencies.Num() > 0)
		{
			// Create the Plugin Self+Dependencies group node and add the current asset to the Self group.
			FName PluginAndDependenciesGroupName = AssetTable->GetNameForPlugin(PluginIndex);
			TSharedPtr<FPluginAndDependenciesGroupNode> PluginAndDependenciesGroup = MakeShared<FPluginAndDependenciesGroupNode>(PluginAndDependenciesGroupName, AssetTable, PluginIndex);
			ParentGroup.AddChildAndSetParent(PluginAndDependenciesGroup);
			PluginGroup = PluginAndDependenciesGroup->CreateChildren().Get();
			PluginGroup->AddChildAndSetParent(NodePtr);
			PluginIndexToGroupNodeMap.Add(PluginIndex, PluginGroup);
		}
		else
		{
			// Create a simple Plugin group node and add the current asset to it.
			FName PluginGroupName = AssetTable->GetNameForPlugin(PluginIndex);
			TSharedPtr<FPluginSimpleGroupNode> PluginGroupPtr = MakeShared<FPluginSimpleGroupNode>(PluginGroupName, AssetTable, PluginIndex);
			ParentGroup.AddChildAndSetParent(PluginGroupPtr);
			PluginGroup = PluginGroupPtr.Get();
			PluginGroup->AddChildAndSetParent(NodePtr);
			PluginIndexToGroupNodeMap.Add(PluginIndex, PluginGroup);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
