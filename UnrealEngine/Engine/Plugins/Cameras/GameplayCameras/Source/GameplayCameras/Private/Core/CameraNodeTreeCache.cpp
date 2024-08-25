// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CameraNodeTreeCache.h"

void FCameraNodeTreeCache::Build(UCameraNode* InRootNode)
{
	CachedNodes.Reset();

	TArray<UCameraNode*> IterStack;
	IterStack.Push(InRootNode);
	while (!IterStack.IsEmpty())
	{
		UCameraNode* CurNode = IterStack.Pop();

		FCachedNode CachedNode{ CurNode->GetNodeFlags(), CurNode };
		CachedNodes.Add(CachedNode);

		FCameraNodeChildrenView CurChildren = CurNode->GetChildren();
		for (UCameraNode* Child : ReverseIterate(CurChildren))
		{
			IterStack.Push(Child);
		}
	}
}

void FCameraNodeTreeCache::ForEachNode(ECameraNodeFlags FlagFilter, TFunctionRef<void(UCameraNode*)> Callback)
{
	for (FCachedNode& CachedNode : CachedNodes)
	{
		if (EnumHasAllFlags(CachedNode.Flags, FlagFilter))
		{
			Callback(CachedNode.Node);
		}
	}
}

