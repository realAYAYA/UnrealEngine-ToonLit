// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneTree.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, AvalancheSceneTree)

FAvaSceneTree::FAvaSceneTree()
{
	RootNode.OwningTree = this;
}

void FAvaSceneTree::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		UpdateTreeNodes();
	}
}

FAvaSceneTreeNode* FAvaSceneTree::FindTreeNode(const FAvaSceneItem& InItem)
{
	if (InItem.IsValid())
	{
		return ItemTreeMap.Find(InItem);
	}
	return nullptr;
}

const FAvaSceneTreeNode* FAvaSceneTree::FindTreeNode(const FAvaSceneItem& InItem) const
{
	if (InItem.IsValid())
	{
		return ItemTreeMap.Find(InItem);
	}
	return nullptr;
}

const FAvaSceneItem* FAvaSceneTree::GetItemAtIndex(int32 InIndex) const
{
	if (SceneItems.IsValidIndex(InIndex))
	{
		return &SceneItems[InIndex];
	}
	return nullptr;
}

FAvaSceneTreeNode& FAvaSceneTree::GetOrAddTreeNode(const FAvaSceneItem& InItem, const FAvaSceneItem& InParentItem)
{
	if (FAvaSceneTreeNode* const ExistingNode = FindTreeNode(InItem))
	{
		// @bug: this was unused, so commented out
		// ExistingNode->ChildrenIndices;
		return *ExistingNode;
	}

	// If Item Tree Map did not find the Item, Scene Items should not have it too
	checkSlow(!SceneItems.Contains(InItem));

	FAvaSceneTreeNode* ParentNode = FindTreeNode(InParentItem);
	if (!ParentNode)
	{
		ParentNode = &RootNode;
	}

	FAvaSceneTreeNode TreeNode;
	TreeNode.GlobalIndex = SceneItems.Add(InItem);
	TreeNode.LocalIndex  = ParentNode->ChildrenIndices.Add(TreeNode.GlobalIndex);
	TreeNode.ParentIndex = ParentNode->GlobalIndex;
	TreeNode.OwningTree  = this;

	return ItemTreeMap.Add(InItem, MoveTemp(TreeNode));
}

const FAvaSceneTreeNode* FAvaSceneTree::FindLowestCommonAncestor(const TArray<const FAvaSceneTreeNode*>& InItems)
{
	TSet<const FAvaSceneTreeNode*> IntersectedAncestors;

	for (const FAvaSceneTreeNode* Item : InItems)
	{
		const FAvaSceneTreeNode* Parent = Item->GetParentTreeNode();
		TSet<const FAvaSceneTreeNode*> ItemAncestors;

		while (Parent)
		{
			ItemAncestors.Add(Parent);
			Parent = Parent->GetParentTreeNode();
		}

		if (IntersectedAncestors.Num() == 0)
		{
			IntersectedAncestors = ItemAncestors;
		}
		else
		{
			IntersectedAncestors = IntersectedAncestors.Intersect(ItemAncestors);

			if (IntersectedAncestors.Num() == 1)
			{
				break;
			}
		}
	}

	const FAvaSceneTreeNode* LowestCommonAncestor = nullptr;
	for (const FAvaSceneTreeNode* Item : IntersectedAncestors)
	{
		if (!LowestCommonAncestor || Item->CalculateHeight() > LowestCommonAncestor->CalculateHeight())
		{
			LowestCommonAncestor = Item;
		}
	}
	return LowestCommonAncestor;
}

bool FAvaSceneTree::CompareTreeItemOrder(const FAvaSceneTreeNode* InA, const FAvaSceneTreeNode* InB)
{
	if (!InA || !InB)
	{
		return false;
	}
	if (const FAvaSceneTreeNode* LowestCommonAncestor = FindLowestCommonAncestor({InA, InB}))
	{
		const TArray<const FAvaSceneTreeNode*> PathToA = LowestCommonAncestor->FindPath({InA});
		const TArray<const FAvaSceneTreeNode*> PathToB = LowestCommonAncestor->FindPath({InB});

		int32 Index = 0;

		int32 PathAIndex = -1;
		int32 PathBIndex = -1;

		while (PathAIndex == PathBIndex)
		{
			if (!PathToA.IsValidIndex(Index))
			{
				return true;
			}
			if (!PathToB.IsValidIndex(Index))
			{
				return false;
			}

			PathAIndex = PathToA[Index]->GetLocalIndex();
			PathBIndex = PathToB[Index]->GetLocalIndex();
			Index++;
		}
		return PathAIndex < PathBIndex;
	}
	return false;
}

void FAvaSceneTree::Reset()
{
	RootNode.Reset();
	ItemTreeMap.Reset();
	SceneItems.Reset();
}

void FAvaSceneTree::UpdateTreeNodes()
{
	for (TPair<FAvaSceneItem, FAvaSceneTreeNode>& Pair : ItemTreeMap)
	{
		FAvaSceneTreeNode& Node = Pair.Value;
		Node.OwningTree = this;
	}
}

TArray<AActor*> FAvaSceneTree::GetChildActors(AActor* const InParentActor) const
{
	TArray<AActor*> OutActors;

	// Todo: using Typed Outer here as right now the Nodes are saved with the Editor World path.
	// Ideally, these should be storing with the Levels Instead
	UWorld* const ActorWorld = InParentActor->GetTypedOuter<UWorld>();
	check(IsValid(ActorWorld));

	const FAvaSceneTreeNode* const ActorSceneTreeNode = FindTreeNode(FAvaSceneItem(InParentActor, ActorWorld));
	if (ActorSceneTreeNode)
	{
		const TConstArrayView<int32> ChildIndices = ActorSceneTreeNode->GetChildrenIndices();

		OutActors.Reserve(ChildIndices.Num());

		for (int32 ChildIndex : ChildIndices)
		{
			const FAvaSceneItem* const ActorSceneItem = GetItemAtIndex(ChildIndex);
			if (ActorSceneItem)
			{
				AActor* const ChildActor = ActorSceneItem->Resolve<AActor>(ActorWorld);
				OutActors.Add(ChildActor);
			}
		}
	}

	return OutActors;
}