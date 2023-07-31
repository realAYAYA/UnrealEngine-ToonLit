// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphCompiler.h"
#include "PCGGraph.h"
#include "PCGEdge.h"
#include "PCGSubgraph.h"
#include "PCGSettings.h" // Needed for trivial element

TArray<FPCGGraphTask> FPCGGraphCompiler::CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId)
{
	if (!InGraph)
	{
		return TArray<FPCGGraphTask>();
	}

	TArray<FPCGGraphTask> CompiledTasks;
	TMap<const UPCGNode*, FPCGTaskId> IdMapping;
	TArray<const UPCGNode*> NodeQueue;

	// Prime the node queue with all nodes that have no inbound edges
	for (const UPCGNode* Node : InGraph->GetNodes())
	{
		if(!Node->HasInboundEdges())
		{
			NodeQueue.Add(Node);
		}
	}

	// By definition, the input node has no inbound edge.
	// Put it last in the queue so it gets picked up first - order is important for hooking up the fetch input element
	NodeQueue.Add(InGraph->GetInputNode());

	while (NodeQueue.Num() > 0)
	{
		const UPCGNode* Node = NodeQueue.Pop();

		const UPCGBaseSubgraphNode* SubgraphNode = Cast<const UPCGBaseSubgraphNode>(Node);
		UPCGGraph* Subgraph = SubgraphNode ? SubgraphNode->GetSubgraph() : nullptr;

		if (SubgraphNode != nullptr && !SubgraphNode->bDynamicGraph && Subgraph)
		{
			const FPCGTaskId PreId = NextId++;

			// 1. Compile the subgraph making sure we don't reuse the same ids
			// Note that we will not consume the pre or post-execute tasks, ergo bIsTopGraph=false
			TArray<FPCGGraphTask> Subtasks = GetCompiledTasks(Subgraph, /*bIsTopGraph=*/false);

#if WITH_EDITOR
			GraphDependenciesLock.Lock();
			GraphDependencies.AddUnique(Subgraph, InGraph);
			GraphDependenciesLock.Unlock();
#endif // WITH_EDITOR

			OffsetNodeIds(Subtasks, NextId);
			NextId += Subtasks.Num();

			const UPCGNode* SubgraphInputNode = Subgraph->GetInputNode();
			const UPCGNode* SubgraphOutputNode = Subgraph->GetOutputNode();

			// 2. Update the "input" and "output" node tasks so we can add the proper dependencies
			FPCGGraphTask* InputNodeTask = Subtasks.FindByPredicate([SubgraphInputNode](const FPCGGraphTask& Subtask) {
				return Subtask.Node == SubgraphInputNode;
				});

			FPCGGraphTask* OutputNodeTask = Subtasks.FindByPredicate([SubgraphOutputNode](const FPCGGraphTask& Subtask) {
				return Subtask.Node == SubgraphOutputNode;
				});

			// Build pre-task
			FPCGGraphTask& PreTask = CompiledTasks.Emplace_GetRef();
			PreTask.Node = Node;
			PreTask.NodeId = PreId;

			for (const UPCGPin* InputPin : Node->InputPins)
			{
				check(InputPin);
				for (const UPCGEdge* InboundEdge : InputPin->Edges)
				{
					if (InboundEdge->IsValid())
					{
						PreTask.Inputs.Emplace(IdMapping[InboundEdge->InputPin->Node], InboundEdge->InputPin, InboundEdge->OutputPin);
					}
					else
					{
						UE_LOG(LogPCG, Warning, TEXT("Invalid inbound edge on subgraph"));
					}
				}
			}

			// Add pre-task as input to subgraph input node task
			if (InputNodeTask)
			{
				InputNodeTask->Inputs.Emplace(PreId, nullptr, nullptr);
			}

			// Merge subgraph tasks into current tasks.
			CompiledTasks.Append(Subtasks);

			// Build post-task
			const FPCGTaskId PostId = NextId++;
			FPCGGraphTask& PostTask = CompiledTasks.Emplace_GetRef();
			PostTask.Node = Node;
			PostTask.NodeId = PostId;

			// Add subgraph output node task as input to the post-task
			if (OutputNodeTask)
			{
				PostTask.Inputs.Emplace(OutputNodeTask->NodeId, nullptr, nullptr);
			}

			check(!IdMapping.Contains(Node));
			IdMapping.Add(Node, PostId);
		}
		else
		{
			const FPCGTaskId NodeId = NextId++;
			FPCGGraphTask& Task = CompiledTasks.Emplace_GetRef();
			Task.Node = Node;
			Task.NodeId = NodeId;

			for (const UPCGPin* InputPin : Node->InputPins)
			{
				for (const UPCGEdge* InboundEdge : InputPin->Edges)
				{
					if (!InboundEdge->IsValid())
					{
						UE_LOG(LogPCG, Warning, TEXT("Unbound inbound edge"));
						continue;
					}

					if (FPCGTaskId* InboundId = IdMapping.Find(InboundEdge->InputPin->Node))
					{
						Task.Inputs.Emplace(IdMapping[InboundEdge->InputPin->Node], InboundEdge->InputPin, InboundEdge->OutputPin); 
					}
					else
					{
						UE_LOG(LogPCG, Error, TEXT("Inconsistent node linkage on node %s"), *Node->GetFName().ToString());
						return TArray<FPCGGraphTask>();
					}
				}
			}

			check(!IdMapping.Contains(Node));
			IdMapping.Add(Node, NodeId);
		}

		// Push next ready nodes on the queue
		for (const UPCGPin* OutPin : Node->OutputPins)
		{
			for (const UPCGEdge* OutboundEdge : OutPin->Edges)
			{
				if (!OutboundEdge->IsValid())
				{
					UE_LOG(LogPCG, Warning, TEXT("Unbound outbound edge"));
					continue;
				}

				const UPCGNode* OutboundNode = OutboundEdge->OutputPin->Node;
				check(OutboundNode);

				if (NodeQueue.Contains(OutboundNode))
				{
					continue;
				}

				bool bAllPrerequisitesMet = true;

				for (const UPCGPin* OutboundNodeInputPin : OutboundNode->InputPins)
				{
					for (const UPCGEdge* OutboundNodeInboundEdge : OutboundNodeInputPin->Edges)
					{
						if (OutboundNodeInboundEdge->IsValid())
						{
							bAllPrerequisitesMet &= IdMapping.Contains(OutboundNodeInboundEdge->InputPin->Node);
						}
					}
				}

				if (bAllPrerequisitesMet)
				{
					NodeQueue.Add(OutboundNode);
				}
			}
		}
	}

	return CompiledTasks;
}

void FPCGGraphCompiler::Compile(UPCGGraph* InGraph)
{
	GraphToTaskMapLock.ReadLock();
	bool bAlreadyCached = GraphToTaskMap.Contains(InGraph);
	GraphToTaskMapLock.ReadUnlock();

	if (bAlreadyCached)
	{
		return;
	}

	// Otherwise, do the compilation; note that we always start at zero since
	// the caller will offset the ids as needed
	FPCGTaskId FirstId = 0;
	TArray<FPCGGraphTask> CompiledTasks = CompileGraph(InGraph, FirstId);

	// TODO: optimize no-ops, etc.

	// Store back the results in the cache if it's valid
	if (!CompiledTasks.IsEmpty())
	{
	GraphToTaskMapLock.WriteLock();
	if (!GraphToTaskMap.Contains(InGraph))
	{
		GraphToTaskMap.Add(InGraph, MoveTemp(CompiledTasks));
	}
	GraphToTaskMapLock.WriteUnlock();
}
}

TArray<FPCGGraphTask> FPCGGraphCompiler::GetCompiledTasks(UPCGGraph* InGraph, bool bIsTopGraph)
{
	TArray<FPCGGraphTask> CompiledTasks;

	if (bIsTopGraph)
	{
		// Always try to compile
		CompileTopGraph(InGraph);

		// Get compiled tasks in a threadsafe way
		GraphToTaskMapLock.ReadLock();
		if (TopGraphToTaskMap.Contains(InGraph))
		{
			CompiledTasks = TopGraphToTaskMap[InGraph];
		}
		GraphToTaskMapLock.ReadUnlock();
	}
	else
	{
		// Always try to compile
		Compile(InGraph);

		// Get compiled tasks in a threadsafe way
		GraphToTaskMapLock.ReadLock();
		if (GraphToTaskMap.Contains(InGraph))
		{
			CompiledTasks = GraphToTaskMap[InGraph];
		}
		GraphToTaskMapLock.ReadUnlock();
	}

	return CompiledTasks;
}

void FPCGGraphCompiler::OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset)
{
	for (FPCGGraphTask& Task : Tasks)
	{
		Task.NodeId += Offset;

		for(FPCGGraphTaskInput& Input : Task.Inputs)
		{
			Input.TaskId += Offset;
		}
	}
}

void FPCGGraphCompiler::CompileTopGraph(UPCGGraph* InGraph)
{
	GraphToTaskMapLock.ReadLock();
	bool bAlreadyCached = TopGraphToTaskMap.Contains(InGraph);
	GraphToTaskMapLock.ReadUnlock();

	if (bAlreadyCached)
	{
		return;
	}

	// Build from non-top tasks
	TArray<FPCGGraphTask> CompiledTasks = GetCompiledTasks(InGraph, /*bIsTopGraph=*/false);

	// Check that the compilation was valid
	if (CompiledTasks.Num() == 0)
	{
		return;
	}

	const int TaskNum = CompiledTasks.Num();
	const FPCGTaskId PreExecuteTaskId = FPCGTaskId(TaskNum);
	const FPCGTaskId PostExecuteTaskId = PreExecuteTaskId + 1;

	FPCGGraphTask& PreExecuteTask = CompiledTasks.Emplace_GetRef();
	PreExecuteTask.Element = MakeShared<FPCGTrivialElement>();
	PreExecuteTask.NodeId = PreExecuteTaskId;

	for (int TaskIndex = 0; TaskIndex < TaskNum; ++TaskIndex)
	{
		FPCGGraphTask& Task = CompiledTasks[TaskIndex];
		if (Task.Inputs.IsEmpty())
		{
			Task.Inputs.Emplace(PreExecuteTaskId, nullptr, nullptr);
		}
	}

	FPCGGraphTask& PostExecuteTask = CompiledTasks.Emplace_GetRef();
	PostExecuteTask.Element = MakeShared<FPCGTrivialElement>();
	PostExecuteTask.NodeId = PostExecuteTaskId;

	// Find end nodes, e.g. all nodes that have no successors.
	// In our representation we don't have this, so find it out by going backwards
	// Note: this works because there is a weak ordering on the tasks such that
	// a successor task is always after its predecessors
	TSet<FPCGTaskId> TasksWithSuccessors;
	const UPCGNode* GraphOutputNode = InGraph->GetOutputNode();

	for (int TaskIndex = TaskNum - 1; TaskIndex >= 0; --TaskIndex)
	{
		const FPCGGraphTask& Task = CompiledTasks[TaskIndex];
		if (!TasksWithSuccessors.Contains(Task.NodeId))
		{
			// For the post task, only the output node will provide data. 
			// It is necessary for any post generation task to get the content of the output node
			// and only this content.
			const bool bProvideData = Task.Node == GraphOutputNode;
			PostExecuteTask.Inputs.Emplace(Task.NodeId, nullptr, nullptr, bProvideData);
		}

		for (const FPCGGraphTaskInput& Input : Task.Inputs)
		{
			TasksWithSuccessors.Add(Input.TaskId);
		}
	}

	// Store back the results in the cache
	GraphToTaskMapLock.WriteLock();
	if (!TopGraphToTaskMap.Contains(InGraph))
	{
		TopGraphToTaskMap.Add(InGraph, MoveTemp(CompiledTasks));
	}
	GraphToTaskMapLock.WriteUnlock();
}

#if WITH_EDITOR
void FPCGGraphCompiler::NotifyGraphChanged(UPCGGraph* InGraph)
{
	if (InGraph)
	{
		RemoveFromCache(InGraph);
	}
}

void FPCGGraphCompiler::RemoveFromCache(UPCGGraph* InGraph)
{
	check(InGraph);
	RemoveFromCacheRecursive(InGraph);
}

void FPCGGraphCompiler::RemoveFromCacheRecursive(UPCGGraph* InGraph)
{
	GraphToTaskMapLock.WriteLock();
	GraphToTaskMap.Remove(InGraph);
	TopGraphToTaskMap.Remove(InGraph);
	GraphToTaskMapLock.WriteUnlock();

	GraphDependenciesLock.Lock();
	TArray<UPCGGraph*> ParentGraphs;
	GraphDependencies.MultiFind(InGraph, ParentGraphs);
	GraphDependencies.Remove(InGraph);
	GraphDependenciesLock.Unlock();

	for (UPCGGraph* Graph : ParentGraphs)
	{
		RemoveFromCacheRecursive(Graph);
	}
}
#endif // WITH_EDITOR
