// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemAllocGroupingByTag.h"

#include "Common/ProviderLock.h" // TraceServices

// Insights
#include "Insights/Common/AsyncOperationProgress.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocNode.h"

#define LOCTEXT_NAMESPACE "Insights::FMemAllocGroupingByTag"

#define INSIGHTS_MERGE_MEM_TAGS_BY_NAME 1

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FMemAllocGroupingByTag);

////////////////////////////////////////////////////////////////////////////////////////////////////

FMemAllocGroupingByTag::FMemAllocGroupingByTag(const TraceServices::IAllocationsProvider& InTagProvider)
	: FTreeNodeGrouping(
		LOCTEXT("Grouping_ByTag_ShortName", "Tag"),
		LOCTEXT("Grouping_ByTag_TitleName", "By Tag"),
		LOCTEXT("Grouping_Tag_Desc", "Creates a tree based on Tag hierarchy."),
		TEXT("Icons.Group.TreeItem"),
		nullptr)
	, TagProvider(InTagProvider)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemAllocGroupingByTag::GroupNodes(const TArray<FTableTreeNodePtr>& Nodes,
                                        FTableTreeNode& ParentGroup, TWeakPtr<FTable> InParentTable,
                                        IAsyncOperationProgress& InAsyncOperationProgress) const
{
	using namespace TraceServices;

	ParentGroup.ClearChildren();

	constexpr TagIdType InvalidTagId = ~0u;
	struct FTagNode
	{
		FTagNode(FName InName, TagIdType InId, TagIdType InParentId, FTableTreeNodePtr InTreeNode)
			: Name(InName), Id(InId), ParentId(InParentId), TreeNode(InTreeNode), Parent(nullptr), AllocCount(0)
		{}

		void MergeTagNodesByName(TMap<TagIdType, FTagNode*>& IdToNodeMap)
		{
			const int32 ChildrenCount = Children.Num();
			for (int32 Index = 0; Index < ChildrenCount; ++Index)
			{
				FTagNode* CurrentNode = Children[Index];
				for (int32 TargetIndex = 0; TargetIndex < Index; ++TargetIndex)
				{
					FTagNode* TargetNode = Children[TargetIndex];
					if (TargetNode->Name == CurrentNode->Name)
					{
						// Move children from CurrentNode to TargetNode.
						for (FTagNode* Child : CurrentNode->Children)
						{
							TargetNode->Children.Add(Child);
							Child->Parent = TargetNode;
						}
						CurrentNode->Children.Reset();

						// Allocs with CurrentNode's tag id will go to TargetNode.
						IdToNodeMap.Emplace(CurrentNode->Id, TargetNode);

						break;
					}
				}
			}
			for (FTagNode* Child : Children)
			{
				Child->MergeTagNodesByName(IdToNodeMap);
			}
		};

		FName Name;

		TagIdType Id;
		TagIdType ParentId;
		FTableTreeNodePtr TreeNode;

		FTagNode* Parent;
		TArray<FTagNode*> Children;
		uint64 AllocCount;
	};
	TArray<FTagNode*> TagNodes;
	TMap<TagIdType, FTagNode*> IdToNodeMap;

	class FScopedMemory
	{
	public:
		FScopedMemory(TArray<FTagNode*>& InTagNodes)
			: TagNodes(InTagNodes)
		{
		}
		~FScopedMemory()
		{
			for (FTagNode* TagNode : TagNodes)
			{
				delete TagNode;
			}
		}

	private:
		TArray<FTagNode*>& TagNodes;
	};
	FScopedMemory _(TagNodes);

	const FSlateBrush* IconBrush = FBaseTreeNode::GetDefaultIcon(true);
	FLinearColor GroupNodeColor(0.75f, 0.5f, 1.0f, 1.0f);

	// Create tag nodes.
	{
		TraceServices::FProviderReadScopeLock TagProviderReadScopeLock(TagProvider);
		TagProvider.EnumerateTags([&](const TCHAR* Name, const TCHAR* FullName, TagIdType Id, TagIdType ParentId)
			{
				const FName NodeName(Name);
#if INSIGHTS_MERGE_MEM_TAGS_BY_NAME
				FTableTreeNodePtr TreeNode = MakeShared<FCustomTableTreeNode>(NodeName, InParentTable, IconBrush, GroupNodeColor);
#else
				const FName NodeNameEx(Name, int32(Id));
				FTableTreeNodePtr TreeNode = MakeShared<FCustomTableTreeNode>(NodeNameEx, InParentTable, IconBrush, GroupNodeColor);
#endif
				FTagNode* TagNode = new FTagNode(NodeName, Id, ParentId, TreeNode);
				TagNodes.Add(TagNode);
				IdToNodeMap.Add(Id, TagNode);
			});
	}

	// Create the Untagged node (if not already present).
	constexpr TagIdType UntaggedNodeId = 0;
	FTagNode* UntaggedNode = IdToNodeMap.FindRef(UntaggedNodeId);
	if (!UntaggedNode)
	{
		const FName NodeName(TEXT("Untagged"));
		FTableTreeNodePtr UntaggedTreeNode = MakeShared<FCustomTableTreeNode>(NodeName, InParentTable, IconBrush, GroupNodeColor);
		UntaggedNode = new FTagNode(NodeName, UntaggedNodeId, InvalidTagId, UntaggedTreeNode);
		TagNodes.Add(UntaggedNode);
		IdToNodeMap.Add(UntaggedNodeId, UntaggedNode);
	}

	// Create the Unknown tag node.
	constexpr TagIdType UnknownTagNodeId = ~0u - 1;
	FTagNode* UnknownTagNode = IdToNodeMap.FindRef(UnknownTagNodeId);
	if (!UnknownTagNode)
	{
		const FName NodeName(TEXT("Unknown"));
		FTableTreeNodePtr UnknownTagTreeNode = MakeShared<FCustomTableTreeNode>(NodeName, InParentTable, IconBrush, GroupNodeColor);
		UnknownTagNode = new FTagNode(NodeName, UnknownTagNodeId, InvalidTagId, UnknownTagTreeNode);
		TagNodes.Add(UnknownTagNode);
		IdToNodeMap.Add(UnknownTagNodeId, UnknownTagNode);
	}

	// Create the virtual tree of tag group nodes.
	FTagNode Root(FName(TEXT("Root")), InvalidTagId, InvalidTagId, nullptr);
	for (FTagNode* TagNode : TagNodes)
	{
		if (TagNode->ParentId == InvalidTagId)
		{
			TagNode->Parent = &Root;
		}
		else
		{
			TagNode->Parent = IdToNodeMap.FindRef(TagNode->ParentId);
			if (!TagNode->Parent)
			{
				TagNode->Parent = UnknownTagNode;
			}
		}

		TagNode->Parent->Children.Add(TagNode);
	}

	if (InAsyncOperationProgress.ShouldCancelAsyncOp())
	{
		return;
	}

#if INSIGHTS_MERGE_MEM_TAGS_BY_NAME
	// Merge tag group nodes (with same parent) by name.
	Root.MergeTagNodesByName(IdToNodeMap);
#endif

	// Add nodes to the correct tag group.
	for (FTableTreeNodePtr NodePtr : Nodes)
	{
		if (InAsyncOperationProgress.ShouldCancelAsyncOp())
		{
			return;
		}

		if (NodePtr->IsGroup())
		{
			ParentGroup.AddChildAndSetParent(NodePtr);
			continue;
		}

		const FMemAllocNode& MemAllocNode = static_cast<const FMemAllocNode&>(*NodePtr);
		const FMemoryAlloc* Alloc = MemAllocNode.GetMemAlloc();
		if (Alloc)
		{
			const TagIdType TagId = Alloc->GetTagId();
			FTagNode* TagNode = IdToNodeMap.FindRef(TagId);
			if (!TagNode)
			{
				TagNode = UnknownTagNode;
			}
			TagNode->TreeNode->AddChildAndSetParent(NodePtr);
			TagNode->AllocCount++;
		}
		else
		{
			ParentGroup.AddChildAndSetParent(NodePtr);
		}
	}

	if (InAsyncOperationProgress.ShouldCancelAsyncOp())
	{
		return;
	}

	for (FTagNode* TagNode : TagNodes)
	{
		FTagNode* Parent = TagNode->Parent;
		while (Parent)
		{
			Parent->AllocCount += TagNode->AllocCount;
			Parent = Parent->Parent;
		}
	}

	if (InAsyncOperationProgress.ShouldCancelAsyncOp())
	{
		return;
	}

	// Fixup parent relationships and filter out empty nodes.
	for (FTagNode* TagNode : TagNodes)
	{
		if (TagNode->AllocCount > 0)
		{
			check(TagNode->TreeNode);
			if (TagNode->Parent->TreeNode)
			{
				TagNode->Parent->TreeNode->AddChildAndSetParent(TagNode->TreeNode);
			}
			else
			{
				check(TagNode->Parent == &Root);
				ParentGroup.AddChildAndSetParent(TagNode->TreeNode);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef INSIGHTS_MERGE_MEM_TAGS_BY_NAME
#undef LOCTEXT_NAMESPACE

} // namespace Insights
