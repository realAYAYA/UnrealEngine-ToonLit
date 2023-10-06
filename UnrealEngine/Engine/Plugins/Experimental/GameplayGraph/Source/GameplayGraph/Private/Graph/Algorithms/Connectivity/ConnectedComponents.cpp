// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Algorithms/Connectivity/ConnectedComponents.h"
#include "Graph/Algorithms/Search/Search.h"

namespace Graph::Algorithms
{

	TArray<TSet<FGraphVertexHandle>> FindConnectedComponents(const TSet<FGraphVertexHandle>& StartingSet)
	{
		TArray<TSet<FGraphVertexHandle>> Output;
		// Do a BFS from each node already assigned to an island.
		// Every node that we hit that's also in the StartingSet gets
		// added to the connected component we're in the middle of
		// building. When the traversal stops, and there's still
		// un-touched nodes in the starting set, we create a new
		// connected component and start again.
		TSet<FGraphVertexHandle> WorkQueue = StartingSet;
		while (!WorkQueue.IsEmpty())
		{
			TSet<FGraphVertexHandle> CurrentComponent;
			BFS(
				*WorkQueue.CreateConstIterator(),
				[&CurrentComponent, &WorkQueue, &StartingSet](const FGraphVertexHandle& Handle)
				{
					if (StartingSet.Contains(Handle))
					{
						CurrentComponent.Add(Handle);
					}
					WorkQueue.Remove(Handle);
					return false;
				}
			);

			Output.Add(CurrentComponent);
		}

		return Output;
	}

}