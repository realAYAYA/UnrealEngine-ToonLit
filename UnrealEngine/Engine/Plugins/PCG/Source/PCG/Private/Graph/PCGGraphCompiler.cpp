// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/PCGGraphCompiler.h"

#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"
#include "Elements/PCGHiGenGridSize.h"
#include "Elements/PCGReroute.h"
#include "Graph/PCGGraphExecutor.h"
#include "Graph/PCGPinDependencyExpression.h"

#include "HAL/IConsoleManager.h"
#include "Misc/ScopeRWLock.h"

namespace PCGGraphCompiler
{
	TAutoConsoleVariable<bool> CVarEnableTaskStaticCulling(
		TEXT("pcg.GraphExecution.TaskStaticCulling"),
		true,
		TEXT("Enable static culling of tasks which considers static branches, generation grid size, trivial nodes and more."));
}

TArray<FPCGGraphTask> FPCGGraphCompiler::CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId, FPCGStackContext& InOutStackContext)
{
	if (!InGraph)
	{
		return TArray<FPCGGraphTask>();
	}

	InOutStackContext.PushFrame(InGraph);

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
		const UPCGBaseSubgraphSettings* SubgraphSettings = SubgraphNode ? Cast<const UPCGBaseSubgraphSettings>(SubgraphNode->GetSettings()) : nullptr;

		// Note that recursive graphs must be dynamic by definition, as we aren't able to 'finish' emitting compiled tasks during compilation otherwise.
		// Implementation note: it is very important that the predicate here is symmetrical with the one in the subgraph node otherwise some tasks could be missing.
		const bool bIsRecursiveGraph = Subgraph && Subgraph->Contains(InGraph);
		const bool bIsNonDynamic = SubgraphSettings && !SubgraphSettings->IsDynamicGraph();
		const bool bIsNotDisabled = SubgraphSettings && SubgraphSettings->bEnabled;

		if(Subgraph && bIsNonDynamic && bIsNotDisabled && !bIsRecursiveGraph)
		{
			const FPCGTaskId PreId = NextId++;

			// 1. Compile the subgraph making sure we don't reuse the same ids
			// Note that we will not consume the pre or post-execute tasks, ergo bIsTopGraph=false
			FPCGStackContext SubgraphStackContext;
			// Passed uninitialized grid size to get all tasks
			TArray<FPCGGraphTask> Subtasks = GetCompiledTasks(Subgraph, PCGHiGenGrid::UninitializedGridSize(), SubgraphStackContext, /*bIsTopGraph=*/false);

#if WITH_EDITOR
			GraphDependenciesLock.Lock();
			GraphDependencies.AddUnique(Subgraph, InGraph);
			GraphDependenciesLock.Unlock();
#endif // WITH_EDITOR

			// Append all the stack frames from inside the subgraph to my current graph stack
			InOutStackContext.PushFrame(Node);
			const int32 StackOffset = InOutStackContext.GetNumStacks();
			for (FPCGGraphTask& Subtask : Subtasks)
			{
				Subtask.StackIndex += StackOffset;
			}
			InOutStackContext.AppendStacks(SubgraphStackContext);
			InOutStackContext.PopFrame();

			OffsetNodeIds(Subtasks, NextId, PreId);
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
			PreTask.StackIndex = InOutStackContext.GetCurrentStackIndex();

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

			// Hook nodes to the PreTask if they require so.
			// Only do it for nodes that are directly under the subgraph, not in subsequent subgraphs.
			for (FPCGGraphTask& Subtask : Subtasks)
			{
				if (!Subtask.Node || Subtask.Node->GetOuter() != Subgraph)
				{
					continue;
				}

				// We hook to pretask if the element requires data from the pretask, or otherwise if there are no inputs, to ensure
				// that the element executes with the other subgraph tasks (and can be culled if the subgraph node is culled).
				const UPCGSettings* Settings = Subtask.Node->GetSettings();
				const bool bRequiresDataFromPreTask = Settings && Settings->RequiresDataFromPreTask();
				if (bRequiresDataFromPreTask || Subtask.Inputs.IsEmpty())
				{
					Subtask.Inputs.Emplace(PreId, /*InInboundPin=*/nullptr, /*InOutboundPin=*/nullptr, bRequiresDataFromPreTask);
				}
			}

			// Merge subgraph tasks into current tasks.
			CompiledTasks.Append(Subtasks);

			// Build post-task
			const FPCGTaskId PostId = NextId++;
			FPCGGraphTask& PostTask = CompiledTasks.Emplace_GetRef();
			PostTask.Node = Node;
			PostTask.NodeId = PostId;
			PostTask.StackIndex = InOutStackContext.GetCurrentStackIndex();
			// Implementation note: since we`ve already executed the node once, we normally don`t need to execute it a second time
			// especially since we cannot distinguish between the pre and post during execution so any data filtering related to pins is bound to fail.
			PostTask.Element = GetSharedTrivialElement();

			// Add execution-only dependency on pre-task, without this post task can be scheduled concurrently with pre-task, and concurrently
			// with something that might become inactive and would then fail to dynamically cull this already-scheduled task.
			// Additional implementation note: this first depedencency is critical in our ability to do static culling (see CalculateStaticallyActiveRecursive)
			// and should not be changed here without changing the other.
			PostTask.Inputs.Emplace(PreId, /*InInboundPin=*/nullptr, /*InOutboundPin=*/nullptr, /*bInProvideData=*/false);

			// Add subgraph output node task as input to the post-task
			if (OutputNodeTask)
			{
				PostTask.Inputs.Emplace(OutputNodeTask->NodeId, /*InInboundPin=*/nullptr, /*InOutboundPin=*/nullptr);
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
			Task.StackIndex = InOutStackContext.GetCurrentStackIndex();

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
						Task.Inputs.Emplace(*InboundId, InboundEdge->InputPin, InboundEdge->OutputPin); 
					}
					else
					{
						UE_LOG(LogPCG, Error, TEXT("Inconsistent node linkage on node '%s'"), *Node->GetFName().ToString());
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
	FPCGStackContext StackContext;
	TArray<FPCGGraphTask> CompiledTasks = CompileGraph(InGraph, FirstId, StackContext);

	// TODO: optimize no-ops, etc.

	// Store back the results in the cache if it's valid
	if (!CompiledTasks.IsEmpty())
	{
		FWriteScopeLock Lock(GraphToTaskMapLock);
		if (!GraphToTaskMap.Contains(InGraph))
		{
			GraphToTaskMap.Add(InGraph, MoveTemp(CompiledTasks));
			GraphToStackContext.Add(InGraph, StackContext);
		}
	}
}

TArray<FPCGGraphTask> FPCGGraphCompiler::GetPrecompiledTasks(const UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph) const
{
	// Get compiled tasks in a threadsafe way
	FReadScopeLock ReadLock(GraphToTaskMapLock);

	const TArray<FPCGGraphTask>* ExistingTasks = nullptr;
	if (bIsTopGraph)
	{
		// Top graphs are optimized per grid size.
		const TMap<uint32, TArray<FPCGGraphTask>>* GridSizeToCompiledGraph = TopGraphToTaskMap.Find(InGraph);
		ExistingTasks = GridSizeToCompiledGraph ? GridSizeToCompiledGraph->Find(GenerationGridSize) : nullptr;

		const TMap<uint32, FPCGStackContext>* GridSizeToStackContext = TopGraphToStackContextMap.Find(InGraph);
		const FPCGStackContext* ExistingStackContext = GridSizeToStackContext ? GridSizeToStackContext->Find(GenerationGridSize) : nullptr;
		OutStackContext = ExistingStackContext ? *ExistingStackContext : FPCGStackContext();

		// Should either find both, or find neither.
		ensure(!ExistingTasks == !ExistingStackContext);
	}
	else
	{
		ExistingTasks = GraphToTaskMap.Find(InGraph);
	}

	return ExistingTasks ? *ExistingTasks : TArray<FPCGGraphTask>();
}

void FPCGGraphCompiler::ResolveGridSizes(
	EPCGHiGenGrid GenerationGrid,
	const TArray<FPCGGraphTask>& CompiledTasks,
	const FPCGStackContext& StackContext,
	EPCGHiGenGrid GenerationDefaultGrid,
	TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompiler::ResolveGridSizes);

	if (CompiledTasks.IsEmpty())
	{
		return;
	}
	check(!InOutTaskGenerationGrid.IsEmpty());

	// Special case - input node must always be present. Execute it on the current grid.
	InOutTaskGenerationGrid[0] = GenerationGrid;

	// Calculate execution grid values for subsequent tasks.
	for (int32 i = 1; i < CompiledTasks.Num(); ++i)
	{
		CalculateGridRecursive(CompiledTasks[i].NodeId, GenerationDefaultGrid, StackContext, CompiledTasks, InOutTaskGenerationGrid);
	}
}

void FPCGGraphCompiler::CreateGridLinkages(
	EPCGHiGenGrid InGenerationGrid,
	TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid,
	TArray<FPCGGraphTask>& InOutCompiledTasks,
	const FPCGStackContext& InStackContext)
{
	// Now add link tasks - if a Grid256 task depends on data from a Grid512 task, inject a link
	// task that looks up the Grid512 component, schedules its execution if it does not have data, and
	// then uses its output data.
	// 
	// The stack is used to form the ResourceKey - a string that provides a path to the data from top graph down to specific pin.
	// This will be used by link tasks as store/retrieve keys to marshal data for edges that cross grid size boundaries.
	const FPCGStack* CurrentStack = InStackContext.GetStack(InStackContext.GetCurrentStackIndex());
	if (InOutCompiledTasks.IsEmpty() || !ensure(CurrentStack))
	{
		return;
	}

	const int32 NumCompiledTasksBefore = InOutCompiledTasks.Num();
	for (FPCGTaskId TaskId = 0; TaskId < NumCompiledTasksBefore; ++TaskId)
	{
		const EPCGHiGenGrid GraphGenerationGrid = InOutTaskGenerationGrid[TaskId];
		for (FPCGGraphTaskInput& TaskInput : InOutCompiledTasks[TaskId].Inputs)
		{
			if (!TaskInput.InPin)
			{
				// Don't link if we don't have a upstream pin to retrieve data from
				continue;
			}

			const EPCGHiGenGrid InputGraphGenerationGrid = InOutTaskGenerationGrid[TaskInput.TaskId];
			// Register linkage task if grid sizes don't match - either way! This allows us to generate an execution-time error if going from
			// small grid to large grid.
			if (InputGraphGenerationGrid != EPCGHiGenGrid::Uninitialized && InputGraphGenerationGrid > GraphGenerationGrid)
			{
				// Build a string identifier for the data
				FString ResourceKey;
				if (!ensure(CurrentStack->CreateStackFramePath(ResourceKey, TaskInput.InPin->Node, TaskInput.InPin)))
				{
					continue;
				}

				// Build task & element to hold the operation to perform
				FPCGGraphTask& LinkTask = InOutCompiledTasks.Emplace_GetRef();
				LinkTask.NodeId = InOutCompiledTasks.Num() - 1;
				LinkTask.StackIndex = InOutCompiledTasks[TaskId].StackIndex;

				LinkTask.Inputs.Emplace(TaskInput.TaskId, TaskInput.InPin, nullptr, /*bConsumeInputData=*/true);

				const EPCGHiGenGrid FromGrid = InOutTaskGenerationGrid[TaskInput.TaskId];
				const EPCGHiGenGrid ToGrid = InOutTaskGenerationGrid[TaskId];

				// This lambda runs at execution time and attempts to retrieve the data from a larger grid. Capture by value is intentional.
				auto GridLinkageOperation = [FromGrid, ToGrid, ResourceKey, OutputPinLabel = TaskInput.InPin->Properties.Label,
					DownstreamNode = InOutCompiledTasks[TaskId].Node, InGenerationGrid](FPCGContext* InContext)
				{
					return PCGGraphExecutor::ExecuteGridLinkage(
						InGenerationGrid,
						FromGrid,
						ToGrid,
						ResourceKey,
						OutputPinLabel,
						DownstreamNode,
						static_cast<FPCGGridLinkageContext*>(InContext));
				};

				FPCGGenericElement::FContextAllocator ContextAllocator = [](const FPCGDataCollection&, TWeakObjectPtr<UPCGComponent>, const UPCGNode*)
				{
					return new FPCGGridLinkageContext();
				};

				LinkTask.Element = MakeShared<PCGGraphExecutor::FPCGGridLinkageElement>(GridLinkageOperation, ContextAllocator, FromGrid, ToGrid, ResourceKey);

				// Now splice in the new task - redirect the downstream task to grab its input from the link task.
				TaskInput.TaskId = LinkTask.NodeId;

				// The link needs to execute at both FROM grid size (store) and TO grid size (retrieve).
				InOutTaskGenerationGrid.Add(FromGrid | ToGrid);
			}
		}
	}
}

EPCGHiGenGrid FPCGGraphCompiler::CalculateGridRecursive(
	FPCGTaskId InTaskId,
	EPCGHiGenGrid GenerationDefaultGrid,
	const FPCGStackContext& InStackContext,
	const TArray<FPCGGraphTask>& InCompiledTasks,
	TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid)
{
	if (InOutTaskGenerationGrid[InTaskId] != EPCGHiGenGrid::Uninitialized)
	{
		return InOutTaskGenerationGrid[InTaskId];
	}

	const FPCGStack* Stack = InStackContext.GetStack(InCompiledTasks[InTaskId].StackIndex);
	check(Stack);
	const bool bTopLevelGraph = InCompiledTasks[InTaskId].ParentId == InvalidPCGTaskId;

	const UPCGNode* Node = InCompiledTasks[InTaskId].Node;
	const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
	const UPCGHiGenGridSizeSettings* GridSizeSettings = Cast<UPCGHiGenGridSizeSettings>(Settings);

	EPCGHiGenGrid Grid = EPCGHiGenGrid::Uninitialized;

	// Grid Size nodes in the top graph set the execution grid level.
	if (GridSizeSettings && GridSizeSettings->bEnabled && bTopLevelGraph)
	{
		Grid = FMath::Min(GenerationDefaultGrid, GridSizeSettings->GetGrid());
	}
	else
	{
		if (InCompiledTasks[InTaskId].Inputs.IsEmpty())
		{
			if (bTopLevelGraph)
			{
				// Tasks with no inputs in top graph get prescribed the default generation grid.
				Grid = GenerationDefaultGrid;
			}
			else
			{
				// Tasks with no inputs in a subgraph should execute on the same grid as the subgraph node task.
				check(InCompiledTasks[InTaskId].ParentId != InvalidPCGTaskId);
				Grid = CalculateGridRecursive(InCompiledTasks[InTaskId].ParentId, GenerationDefaultGrid, InStackContext, InCompiledTasks, InOutTaskGenerationGrid);
			}
		}
		else
		{
			// This task has inputs. Grid of this task is minimum of all input grids. We can link in data from a larger grid, but
			// not from a finer grid (this goes against hierarchy).
			Grid = EPCGHiGenGrid::Unbounded;
			for (FPCGGraphTaskInput InputTask : InCompiledTasks[InTaskId].Inputs)
			{
				const EPCGHiGenGrid InputGrid = CalculateGridRecursive(InputTask.TaskId, GenerationDefaultGrid, InStackContext, InCompiledTasks, InOutTaskGenerationGrid);
				if (PCGHiGenGrid::IsValidGrid(InputGrid))
				{
					Grid = FMath::Min(InputGrid, Grid);
				}
			}
		}
	}

	ensure(Grid != EPCGHiGenGrid::Uninitialized);

	InOutTaskGenerationGrid[InTaskId] = Grid;

	return Grid;
}

bool FPCGGraphCompiler::CalculateStaticallyActiveRecursive(FPCGTaskId InTaskId, const TArray<FPCGGraphTask>& InCompiledTasks, TMap<int32, bool>& InOutTaskIdToActiveFlag)
{
	if (const bool* bEntry = InOutTaskIdToActiveFlag.Find(InTaskId))
	{
		return *bEntry;
	}

	const UPCGNode* Node = InCompiledTasks[InTaskId].Node;

	// Nodes within subgraphs - if the subgraph node is inactive then all tasks within the subgraph are inactive.
	if (InCompiledTasks[InTaskId].ParentId != InvalidPCGTaskId)
	{
		const bool bParentActive = CalculateStaticallyActiveRecursive(InCompiledTasks[InTaskId].ParentId, InCompiledTasks, InOutTaskIdToActiveFlag);
		// With respect to the input node in a static subgraph, it has to use the same value as its parent since there is no direct edge between the subgraph node and this input node.
		if (!bParentActive || (Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGGraphInputOutputSettings>()))
		{
			InOutTaskIdToActiveFlag.Add(InTaskId, bParentActive);
			return bParentActive;
		}
	}

	if (!Node)
	{
		InOutTaskIdToActiveFlag.Add(InTaskId, true);
		return true;
	}

	// For static subgraphs, the second time this node is seen, it has a trivial task and no proper edge, which will trip the static validation below
	// This should basically forward the same information as the original node - which is conveniently the first input on that task.
	if (UPCGSubgraphSettings* SubgraphSettings = Cast<UPCGSubgraphSettings>(Node->GetSettings()))
	{
		FPCGTaskId TentativeSubgraphInputId = InCompiledTasks[InTaskId].Inputs.IsEmpty() ? InvalidPCGTaskId : InCompiledTasks[InTaskId].Inputs[0].TaskId;

		if (!SubgraphSettings->IsDynamicGraph() && TentativeSubgraphInputId != InvalidPCGTaskId && InCompiledTasks[TentativeSubgraphInputId].Node == Node)
		{
			const bool bSubgraphInputActive = CalculateStaticallyActiveRecursive(TentativeSubgraphInputId, InCompiledTasks, InOutTaskIdToActiveFlag);

			InOutTaskIdToActiveFlag.Add(InTaskId, bSubgraphInputActive);
			return bSubgraphInputActive;
		}
	}

	TArray<FName, TInlineAllocator<8>> PinsRequiringActiveConnection;

	// Three relevant types of input pins - required, non-advanced and advanced. This tracks if we encounter the second category.
	bool bHasAnyNonAdvancedPins = false;

	for (UPCGPin* InputPin : Node->GetInputPins())
	{
		if (!InputPin)
		{
			continue;
		}

		if (!InputPin->Properties.IsAdvancedPin())
		{
			bHasAnyNonAdvancedPins = true;

			if (Node->IsInputPinRequiredByExecution(InputPin))
			{
				PinsRequiringActiveConnection.AddUnique(InputPin->Properties.Label);
			}
		}
	}
	
	bool bHasAnyNonAdvancedInputs = false;
	bool bHasAnyActiveNonAdvancedInput = false;

	for (const FPCGGraphTaskInput& Input : InCompiledTasks[InTaskId].Inputs)
	{
		const UPCGPin* InputPin = Input.OutPin;

		// Only non-advanced input pins play a part in determining active/inactive state.
		if (InputPin && InputPin->Properties.IsAdvancedPin())
		{
			continue;
		}

		bHasAnyNonAdvancedInputs = true;

		// Default to tasks being active unless proved otherwise.
		bool bInputActive = true;

		// If we are connected to an upstream node, evaluate if the output pin is active.
		if (Input.InPin)
		{
			const UPCGNode* UpstreamNode = InCompiledTasks[Input.TaskId].Node;
			if (const UPCGSettings* UpstreamSettings = UpstreamNode ? UpstreamNode->GetSettings() : nullptr)
			{
				bInputActive &= UpstreamSettings->IsPinStaticallyActive(Input.InPin->Properties.Label);
			}
		}

		if (bInputActive)
		{
			bInputActive &= CalculateStaticallyActiveRecursive(Input.TaskId, InCompiledTasks, InOutTaskIdToActiveFlag);
		}

		if (bInputActive)
		{
			bHasAnyActiveNonAdvancedInput = true;

			if (InputPin)
			{
				// Register received input on this pin.
				PinsRequiringActiveConnection.Remove(InputPin->Properties.Label);
			}
		}
	}

	const UPCGSettings* Settings = Node->GetSettings();
	const bool bCanCullIfUnwired = Settings && Settings->CanCullTaskIfUnwired();

	bool bActive = true;

	if (!PinsRequiringActiveConnection.IsEmpty())
	{
		// If PinsRequiringActiveConnection is not empty then we did not find an input for each required pin.
		bActive = false;
	}
	else if (bCanCullIfUnwired)
	{
		// Cull if we have non-advanced pins but we don't have any active non-advanced inputs.
		bActive = !(bHasAnyNonAdvancedPins && !bHasAnyActiveNonAdvancedInput);
	}
	else if (bHasAnyNonAdvancedInputs)
	{
		// This task is allowed to be unwired, so we have non-advanced inputs and they're all inactive - basically
		// all upstream inputs are inactive which forces this task to be inactive.
		bActive = bHasAnyActiveNonAdvancedInput || InCompiledTasks[InTaskId].Inputs.IsEmpty();
	}

	InOutTaskIdToActiveFlag.Add(InTaskId, bActive);

	return bActive;
}

void FPCGGraphCompiler::CullTasksStaticInactive(TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	if (InOutCompiledTasks.IsEmpty())
	{
		return;
	}

	TMap<int32, bool> NodeIdToActiveFlag;
	// First task is input node task which is active
	NodeIdToActiveFlag.Add(InOutCompiledTasks[0].NodeId, true);

	for (int32 i = 1; i < InOutCompiledTasks.Num(); ++i)
	{
		// Results of each call memoized via NodeIdToActiveFlag.
		CalculateStaticallyActiveRecursive(InOutCompiledTasks[i].NodeId, InOutCompiledTasks, NodeIdToActiveFlag);
	}

	auto ShouldCull = [&NodeIdToActiveFlag](const FPCGGraphTask& InTask)
	{
		const bool* bActive = NodeIdToActiveFlag.Find(InTask.NodeId);
		return bActive && !*bActive;
	};
	CullTasks(InOutCompiledTasks, /*bAddPassthroughWires=*/false, ShouldCull);
}

void FPCGGraphCompiler::CullTasks(TArray<FPCGGraphTask>& InOutCompiledTasks, bool bAddPassthroughWires, TFunctionRef<bool(const FPCGGraphTask&)> CullTask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompiler::CullTasks);

	// Check that we have more than just the input task to work with.
	if (InOutCompiledTasks.Num() < 2)
	{
		return;
	}

	TArray<int32> TaskRemapping;
	TaskRemapping.SetNumUninitialized(InOutCompiledTasks.Num());

	// Mark culled tasks by remapping to INDEX_NONE. First task is input task and is never culled.
	TaskRemapping[0] = 0;
	for (int32 TaskIndex = 1; TaskIndex < InOutCompiledTasks.Num(); ++TaskIndex)
	{
		TaskRemapping[TaskIndex] = CullTask(InOutCompiledTasks[TaskIndex]) ? INDEX_NONE : 0;
	}

	// Optionally add wires that bypass culled nodes.
	if (bAddPassthroughWires)
	{
		for (int32 TaskIndex = 1; TaskIndex < InOutCompiledTasks.Num(); ++TaskIndex)
		{
			FPCGGraphTask& Task = InOutCompiledTasks[TaskIndex];
			if (!Task.Node || Task.Node->GetInputPins().IsEmpty())
			{
				continue;
			}

			const int32 InputNumBefore = Task.Inputs.Num();
			for (int32 InputIndex = 0; InputIndex < InputNumBefore; ++InputIndex)
			{
				const int32 InputTaskId = Task.Inputs[InputIndex].TaskId;

				// Is node culled?
				if (TaskRemapping[InputTaskId] == INDEX_NONE)
				{
					const FPCGGraphTask& InputTask = InOutCompiledTasks[InputTaskId];
					if (InputTask.Node && InputTask.Node->GetInputPins().Num() > 1)
					{
						ensureMsgf(false, TEXT("Task culling currently only supports nodes with a single input pin which are trivial to unwire."));
						continue;
					}

					// Upstream node was culled. Wire up the inputs of the culled node to this node.
					for (int32 InputInputIndex = 0; InputInputIndex < InputTask.Inputs.Num(); ++InputInputIndex)
					{
						FPCGGraphTaskInput& NewInput = Task.Inputs.Add_GetRef(InputTask.Inputs[InputInputIndex]);
						NewInput.OutPin = Task.Inputs[InputIndex].OutPin;
					}
				}
			}
		}
	}

	// Remove all culled tasks by compacting the task array. Never cull first (Input) task.
	int32 WriteIndex = 1;
	int32 ReadIndex = 1;
	while (ReadIndex < InOutCompiledTasks.Num())
	{
		// If not culled, then move the task to it's final remapped position in the task array.
		if (TaskRemapping[ReadIndex] != INDEX_NONE)
		{
			if (WriteIndex != ReadIndex)
			{
				InOutCompiledTasks[WriteIndex] = MoveTemp(InOutCompiledTasks[ReadIndex]);
				InOutCompiledTasks[WriteIndex].NodeId = WriteIndex;
			}

			TaskRemapping[ReadIndex] = WriteIndex;
			++WriteIndex;
		}

		++ReadIndex;
	}

	InOutCompiledTasks.SetNum(WriteIndex);

	for (FPCGGraphTask& Task : InOutCompiledTasks)
	{
		// Remap input task IDs, and remove edges that connect to culled nodes.
		for (int32 InputIndex = Task.Inputs.Num() - 1; InputIndex >= 0; --InputIndex)
		{
			const int32 InputTaskId = Task.Inputs[InputIndex].TaskId;
			const int32 Remap = TaskRemapping[InputTaskId];
			if (Remap != INDEX_NONE)
			{
				Task.Inputs[InputIndex].TaskId = Remap;
			}
			else
			{
				Task.Inputs.RemoveAt(InputIndex);
			}
		}

		// Remap parent ID if there tasks is in a child scope.
		if (Task.ParentId != InvalidPCGTaskId)
		{
			const int32 RemappedParentId = TaskRemapping[Task.ParentId];
			// Parent task should not have been culled.
			ensure(RemappedParentId != INDEX_NONE);

			// Write the remapped ID - even if it's invalid/INDEX_NONE. Hanging parent IDs can cause issues elsewhere.
			Task.ParentId = RemappedParentId;
		}
	}
}

void FPCGGraphCompiler::PostCullStackCleanup(TArray<FPCGGraphTask>& InCompiledTasks, FPCGStackContext& InOutStackContext)
{
	// Build set of stack IDs used by tasks. Using array as set is likely small.
	TArray<int32> ActiveStackIDs;
	for (FPCGGraphTask& Task : InCompiledTasks)
	{
		ActiveStackIDs.AddUnique(Task.StackIndex);
	}

	if (ActiveStackIDs.Num() < InOutStackContext.GetNumStacks())
	{
		// Prepare cumulative counts for stack ID remapping.
		TArray<int> RemovedCounts;
		RemovedCounts.SetNumZeroed(InOutStackContext.GetNumStacks());

		// Remove any stacks that are inactive.
		TArray<FPCGStack>& Stacks = InOutStackContext.GetStacksMutable();
		for (int StackIndex = Stacks.Num() - 1; StackIndex >= 0; --StackIndex)
		{
			if (!ActiveStackIDs.Contains(StackIndex))
			{
				Stacks.RemoveAt(StackIndex);
				RemovedCounts[StackIndex] = 1;
			}
		}

		// Accumulate removed counts.
		for (int Index = 1; Index < RemovedCounts.Num(); ++Index)
		{
			RemovedCounts[Index] += RemovedCounts[Index - 1];
		}

		// Remap stack IDs.
		for (FPCGGraphTask& Task : InCompiledTasks)
		{
			Task.StackIndex -= RemovedCounts[Task.StackIndex];
		}
	}
}

void FPCGGraphCompiler::CalculateDynamicActivePinDependencies(FPCGTaskId InTaskId, TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	if (!ensure(InOutCompiledTasks.IsValidIndex(InTaskId)))
	{
		return;
	}

	const UPCGNode* Node = InOutCompiledTasks[InTaskId].Node;

	// First if there are any required pins, build the pin dependencies from these. We have an array of inputs, each one can
	// correspond to a pin. Use a map to compile an expression for each input pin label.
	TMap<FName, FPCGPinDependencyExpression> InputPinLabelToPinDependency;

	for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[InTaskId].Inputs)
	{
		if (!Node || !Input.OutPin)
		{
			continue;
		}

		const UPCGPin* InputPin = Input.OutPin;

		// Consider only primary input pins in this pass.
		if (!Node->IsInputPinRequiredByExecution(InputPin))
		{
			continue;
		}

		const UPCGNode* UpstreamNode = InOutCompiledTasks[Input.TaskId].Node;
		if (!UpstreamNode)
		{
			continue;
		}

		const int PinIndex = UpstreamNode->GetOutputPins().IndexOfByPredicate([&Input](const UPCGPin* InPin)
		{
			return InPin == Input.InPin;
		});

		if (PinIndex != INDEX_NONE)
		{
			check(PinIndex < PCGPinIdHelpers::MaxOutputPins);

			FPCGPinDependencyExpression& PinDependency = InputPinLabelToPinDependency.FindOrAdd(Input.OutPin->Properties.Label);
			PinDependency.AddPinDependency(PCGPinIdHelpers::NodeIdAndPinIndexToPinId(Input.TaskId, PinIndex));
		}
	}

	// Result expression. Conjunction of disjunctions of pin IDs that are required to be active for this task to be active.
	// Example - keep task if: UpstreamPin0Active && (UpstreamPin1Active || UpstreamPin2Active)
	FPCGPinDependencyExpression PinDependency;

	if (!InputPinLabelToPinDependency.IsEmpty())
	{
		// If we have registered pin dependencies from the first pass (required pins) then we have can compose these for our pin
		// dependency expression. Build conjunction from the per-pin disjunctions.
		for (TPair<FName, FPCGPinDependencyExpression>& PinExpression : InputPinLabelToPinDependency)
		{
			PinDependency.AppendUsingConjunction(PinExpression.Value);
		}
	}
	else
	{
		// In the case of output nodes, advanced pins shouldn't be ignored as it can and will prevent culling in some instances
		const bool bTreatAdvancedPinsAsNormal = (Node && Cast<UPCGGraphInputOutputSettings>(Node->GetSettings()) && !Cast<UPCGGraphInputOutputSettings>(Node->GetSettings())->IsInput());

		// If we don't have any dependent pins logged by now then there are no required pins (note that a node that does not have task
		// inputs for required pins will be statically culled in an earlier compilation step). In which case we'll be active if *any* input
		// is active. We build a disjunction that expresses this.
		for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[InTaskId].Inputs)
		{
			if (Input.OutPin && Input.OutPin->Properties.IsAdvancedPin() && !bTreatAdvancedPinsAsNormal)
			{
				// Advanced input pins never participate in keeping node active.
				continue;
			}

			if (const UPCGPin* UpstreamOutputPin = Input.InPin)
			{
				// Input connection is via node pins.
				const UPCGNode* UpstreamNode = InOutCompiledTasks[Input.TaskId].Node;
				if (!UpstreamNode)
				{
					continue;
				}

				const int PinIndex = UpstreamNode->GetOutputPins().IndexOfByPredicate([UpstreamOutputPin](const UPCGPin* InPin)
				{
					return InPin == UpstreamOutputPin;
				});

				if (PinIndex != INDEX_NONE)
				{
					check(PinIndex < PCGPinIdHelpers::MaxOutputPins);

					PinDependency.AddPinDependency(PCGPinIdHelpers::NodeIdAndPinIndexToPinId(Input.TaskId, PinIndex));
				}
			}
			else
			{
				// No associated pin, use a special pin ID for a pin-less dependency.
				PinDependency.AddPinDependency(PCGPinIdHelpers::NodeIdToPinId(Input.TaskId));
			}
		}
	}

	InOutCompiledTasks[InTaskId].PinDependency = MoveTemp(PinDependency);
}

TArray<FPCGGraphTask> FPCGGraphCompiler::GetCompiledTasks(UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph)
{
	TArray<FPCGGraphTask> CompiledTasks;

	if (bIsTopGraph)
	{
		// Always try to compile
		CompileTopGraph(InGraph, GenerationGridSize);

		// Get compiled tasks in a threadsafe way
		FReadScopeLock Lock(GraphToTaskMapLock);
		const TMap<uint32, TArray<FPCGGraphTask>>* GridSizeToCompiledGraph = TopGraphToTaskMap.Find(InGraph);
		const TArray<FPCGGraphTask>* Tasks = GridSizeToCompiledGraph ? GridSizeToCompiledGraph->Find(GenerationGridSize) : nullptr;

		const TMap<uint32, FPCGStackContext>* GridSizeToStackContext = TopGraphToStackContextMap.Find(InGraph);
		const FPCGStackContext* StackContext = GridSizeToStackContext ? GridSizeToStackContext->Find(GenerationGridSize) : nullptr;

		// Should have either found both or neither.
		ensure(!Tasks == !StackContext);

		if (Tasks && StackContext)
		{
			CompiledTasks = *Tasks;
			OutStackContext = *StackContext;
		}
	}
	else
	{
		// Always try to compile
		Compile(InGraph);

		// Get compiled tasks in a threadsafe way
		FReadScopeLock Lock(GraphToTaskMapLock);
		if (TArray<FPCGGraphTask>* Tasks = GraphToTaskMap.Find(InGraph))
		{
			CompiledTasks = *Tasks;
			OutStackContext = GraphToStackContext[InGraph];
		}
	}

	return CompiledTasks;
}

void FPCGGraphCompiler::OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset, FPCGTaskId ParentId)
{
	for (FPCGGraphTask& Task : Tasks)
	{
		Task.NodeId += Offset;

		if (Task.ParentId == InvalidPCGTaskId)
		{
			Task.ParentId = ParentId;
		}
		else
		{
			Task.ParentId += Offset;
		}

		for (FPCGGraphTaskInput& Input : Task.Inputs)
		{
			Input.TaskId += Offset;
		}

		Task.PinDependency.OffsetNodeIds(Offset);
	}
}

void FPCGGraphCompiler::CompileTopGraph(UPCGGraph* InGraph, uint32 GenerationGridSize)
{
	GraphToTaskMapLock.ReadLock();
	const TMap<uint32, TArray<FPCGGraphTask>>* GridSizeToCompiledGraph = TopGraphToTaskMap.Find(InGraph);
	const bool bAlreadyCached = GridSizeToCompiledGraph && GridSizeToCompiledGraph->Contains(GenerationGridSize);
	GraphToTaskMapLock.ReadUnlock();

	if (bAlreadyCached)
	{
		return;
	}

	UE_LOG(LogPCG, Verbose, TEXT("FPCGGraphCompiler::CompileTopGraph '%s' grid: %u"), *InGraph->GetName(), GenerationGridSize);

	// Build from non-top tasks
	FPCGStackContext StackContext;
	TArray<FPCGGraphTask> CompiledTasks = GetCompiledTasks(InGraph, GenerationGridSize, StackContext, /*bIsTopGraph=*/false);

	// Check that the compilation was valid
	if (CompiledTasks.Num() == 0)
	{
		return;
	}

	if (PCGGraphCompiler::CVarEnableTaskStaticCulling.GetValueOnAnyThread())
	{
		// Remove reroute nodes before execution grid setup, as grid linkages need final nodes to connect from/to.
		CullTasks(CompiledTasks, /*bAddPassthroughWires=*/true, [](const FPCGGraphTask& InTask) { return InTask.Node && Cast<UPCGRerouteSettings>(InTask.Node->GetSettings()) && InTask.Node->HasInboundEdges(); });

		// Cull inactive branches downstream of branch nodes with static selection values.
		CullTasksStaticInactive(CompiledTasks);

		// For hierarchical generation resolve the execution grid for each task and cull any tasks that won't execute.
		// TODO - we could add an else branch for higen disabled that culls the grid size nodes.
		if (InGraph->IsHierarchicalGenerationEnabled() && GenerationGridSize != PCGHiGenGrid::UninitializedGridSize())
		{
			const EPCGHiGenGrid GenerationGrid = PCGHiGenGrid::GridSizeToGrid(GenerationGridSize);
			const EPCGHiGenGrid DefaultGrid = PCGHiGenGrid::GridSizeToGrid(InGraph->GetDefaultGridSize());

			// Propagate grid size nodes through the graph to determine which grid size each task should execute on.
			TArray<EPCGHiGenGrid> TaskGenerationGrid;
			TaskGenerationGrid.SetNumZeroed(CompiledTasks.Num());
			ResolveGridSizes(GenerationGrid, CompiledTasks, StackContext, DefaultGrid, TaskGenerationGrid);

			// Create linkage tasks for edges that cross from large grid to small grid tasks.
			CreateGridLinkages(GenerationGrid, TaskGenerationGrid, CompiledTasks, StackContext);

			// Cull any task that should not execute on the current grid.
			CullTasks(CompiledTasks, /*bAddPassthroughWires=*/false, [GenerationGrid, &TaskGenerationGrid](const FPCGGraphTask& InTask)
			{
				const EPCGHiGenGrid TaskGrid = TaskGenerationGrid[InTask.NodeId];
				return TaskGrid != EPCGHiGenGrid::Uninitialized && !(TaskGrid & GenerationGrid);
			});
		}
	}

	// Post culling - remove any stacks that are no longer part of execution. Besides being tidy this also helps
	// debug tools discern which stacks were executed or not.
	PostCullStackCleanup(CompiledTasks, StackContext);

	// To feed dynamic culling at execution time, build list of upstream pins that we depend on (if all of these pins
	// are determined to be inactive at execution time, then the task will be deactivated). An empty list means the
	// task will never be deactivated and will always execute.
	// TODO: Expand pin dependencies to pure branches that aren't directly downstream - nodes that have no side effects
	// and feed into a branch can also be culled if the branch is culled.
	// TODO: Pin dependencies should be transitive across nodes. If a node is dependent on a single upstream node, it could
	// likely take the pin dependencies from the upstream node, which should save iterations in the dynamic culling code.
	for (int TaskIndex = 0; TaskIndex < CompiledTasks.Num(); ++TaskIndex)
	{
		// Result is written directly to tasks.
		CalculateDynamicActivePinDependencies(CompiledTasks[TaskIndex].NodeId, CompiledTasks);
	}

	const int TaskNum = CompiledTasks.Num();
	const FPCGTaskId PreExecuteTaskId = FPCGTaskId(TaskNum);
	const FPCGTaskId PostExecuteTaskId = PreExecuteTaskId + 1;

	FPCGGraphTask& PreExecuteTask = CompiledTasks.Emplace_GetRef();
	PreExecuteTask.Element = GetSharedTrivialElement();
	PreExecuteTask.NodeId = PreExecuteTaskId;

	for (int TaskIndex = 0; TaskIndex < TaskNum; ++TaskIndex)
	{
		FPCGGraphTask& Task = CompiledTasks[TaskIndex];
		if (Task.Inputs.IsEmpty())
		{
			Task.Inputs.Emplace(PreExecuteTaskId, nullptr, nullptr);
		}

		Task.CompiledTaskId = Task.NodeId;
	}

	FPCGGraphTask& PostExecuteTask = CompiledTasks.Emplace_GetRef();
	PostExecuteTask.Element = GetSharedTrivialElement();
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
	TMap<uint32, TArray<FPCGGraphTask>>& TasksPerGenerationGrid = TopGraphToTaskMap.FindOrAdd(InGraph);
	if (!TasksPerGenerationGrid.Contains(GenerationGridSize))
	{
		TasksPerGenerationGrid.Add(GenerationGridSize, MoveTemp(CompiledTasks));
	}

	TMap<uint32, FPCGStackContext>& StackContextPerGenerationGrid = TopGraphToStackContextMap.FindOrAdd(InGraph);
	if (!StackContextPerGenerationGrid.Contains(GenerationGridSize))
	{
		StackContextPerGenerationGrid.Add(GenerationGridSize, MoveTemp(StackContext));
	}
	GraphToTaskMapLock.WriteUnlock();
}

FPCGElementPtr FPCGGraphCompiler::GetSharedTrivialElement()
{
	{
		FReadScopeLock Lock(SharedTrivialElementLock);

		if (SharedTrivialElement)
		{
			return SharedTrivialElement;
		}
	}

	FWriteScopeLock Lock(SharedTrivialElementLock);

	if (!SharedTrivialElement)
	{
		SharedTrivialElement = MakeShared<FPCGTrivialElement>();
	}

	return SharedTrivialElement;
}

void FPCGGraphCompiler::ClearCache()
{
	FWriteScopeLock Lock(GraphToTaskMapLock);
	GraphToTaskMap.Reset();
	GraphToStackContext.Reset();
	TopGraphToTaskMap.Reset();
	TopGraphToStackContextMap.Reset();
}

#if WITH_EDITOR
void FPCGGraphCompiler::NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType)
{
	if (InGraph && (ChangeType != EPCGChangeType::Cosmetic))
	{
		RemoveFromCache(InGraph);
	}
}

bool FPCGGraphCompiler::Recompile(UPCGGraph* InGraph, uint32 GenerationGridSize, bool bIsTopGraph)
{
	FPCGStackContext StackContextBefore;
	const TArray<FPCGGraphTask> TasksBefore = GetPrecompiledTasks(InGraph, GenerationGridSize, StackContextBefore, bIsTopGraph);

	// Need to manually purge as the graph compiler will not have gotten the change notification yet. Editor only.
	RemoveFromCache(InGraph);

	FPCGStackContext StackContextAfter;
	const TArray<FPCGGraphTask> TasksAfter = GetCompiledTasks(InGraph, GenerationGridSize, StackContextAfter, bIsTopGraph);

	bool bAllTasksEqual = TasksBefore.Num() == TasksAfter.Num();
	if (bAllTasksEqual)
	{
		for (int I = 0; I < TasksBefore.Num(); ++I)
		{
			bAllTasksEqual = bAllTasksEqual && TasksAfter[I].IsApproximatelyEqual(TasksBefore[I]);
		}
	}

	// Compiled result is compiled tasks + associated stacks. Compare both and return true if compiled result changes.
	return !bAllTasksEqual || (StackContextBefore != StackContextAfter);
}

void FPCGGraphCompiler::RemoveFromCache(UPCGGraph* InGraph)
{
	UE_LOG(LogPCG, Verbose, TEXT("FPCGGraphCompiler::RemoveFromCache '%s'"), *InGraph->GetName());

	check(InGraph);
	RemoveFromCacheRecursive(InGraph);
}

void FPCGGraphCompiler::RemoveFromCacheRecursive(UPCGGraph* InGraph)
{
	GraphToTaskMapLock.WriteLock();
	GraphToTaskMap.Remove(InGraph);
	GraphToStackContext.Remove(InGraph);
	TopGraphToTaskMap.Remove(InGraph);
	TopGraphToStackContextMap.Remove(InGraph);
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
