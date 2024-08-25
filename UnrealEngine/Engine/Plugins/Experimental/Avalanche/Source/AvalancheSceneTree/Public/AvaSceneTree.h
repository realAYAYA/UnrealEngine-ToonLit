// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSceneItem.h"
#include "AvaSceneTreeNode.h"
#include "Containers/Map.h"
#include "AvaSceneTree.generated.h"

class AActor;

USTRUCT()
struct AVALANCHESCENETREE_API FAvaSceneTree
{
	GENERATED_BODY()

	FAvaSceneTree();

	void PostSerialize(const FArchive& Ar);

	FAvaSceneTreeNode& GetRootNode() { return RootNode; }

	const FAvaSceneTreeNode& GetRootNode() const { return RootNode; }

	FAvaSceneTreeNode* FindTreeNode(const FAvaSceneItem& InItem);

	const FAvaSceneTreeNode* FindTreeNode(const FAvaSceneItem& InItem) const;

	const FAvaSceneItem* GetItemAtIndex(int32 InIndex) const;

	FAvaSceneTreeNode& GetOrAddTreeNode(const FAvaSceneItem& InItem, const FAvaSceneItem& InParentItem);

	static const FAvaSceneTreeNode* FindLowestCommonAncestor(const TArray<const FAvaSceneTreeNode*>& InItems);
	
	static bool CompareTreeItemOrder(const FAvaSceneTreeNode* InA, const FAvaSceneTreeNode* InB);

	void Reset();

	/** Returns all resolved child actors of a specified parent actor in the scene tree. */
	TArray<AActor*> GetChildActors(AActor* const InParentActor) const;

private:
	void UpdateTreeNodes();

	UPROPERTY()
	FAvaSceneTreeNode RootNode;

	UPROPERTY()
	TArray<FAvaSceneItem> SceneItems;

	UPROPERTY()
	TMap<FAvaSceneItem, FAvaSceneTreeNode> ItemTreeMap;
};

template<>
struct TStructOpsTypeTraits<FAvaSceneTree> : TStructOpsTypeTraitsBase2<FAvaSceneTree>
{
	enum 
	{
		WithPostSerialize = true,
	};
};
