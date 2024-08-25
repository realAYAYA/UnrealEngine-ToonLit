// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/CameraNode.h"
#include "Templates/Function.h"

/**
 * A utility class that caches a hierarchy of arbitrary camera nodes as a
 * flat depth-first list of those camera nodes.
 */
class FCameraNodeTreeCache
{
public:

	/** (Re)builds the cache given the root node of a hierarchy. */
	void Build(UCameraNode* InRootNode);

	void ForEachNode(ECameraNodeFlags FlagFilter, TFunctionRef<void(UCameraNode*)> Callback);

private:

	struct FCachedNode
	{
		ECameraNodeFlags Flags;
		TObjectPtr<UCameraNode> Node;
	};
	TArray<FCachedNode> CachedNodes;
};

