// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "Graph/PCGStackContext.h"

class IPCGElement;
class UPCGGraph;
struct FPCGGraphTask;

typedef TSharedPtr<IPCGElement, ESPMode::ThreadSafe> FPCGElementPtr;

/** 
* FPCGGraphCompiler
* This class compiles a graph into tasks and keeps an internal cache
*/
class FPCGGraphCompiler
{
public:
	void Compile(UPCGGraph* InGraph);
	TArray<FPCGGraphTask> GetCompiledTasks(UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph = true);
	TArray<FPCGGraphTask> GetPrecompiledTasks(const UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph = true) const;

	static void OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset, FPCGTaskId ParentId);

#if WITH_EDITOR
	/** Checks whether the compiled result changes when recompiling InGraph. Note that this incurs the cost of a graph compilation each time it is called.
	* The cache will be updated with the latest compiled result. Returns true if the compiled result changes;
	*/
	bool Recompile(UPCGGraph* InGraph, uint32 GenerationGridSize, bool bIsTopGraph = true);

	void RemoveFromCache(UPCGGraph* InGraph);

	void NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType);
#endif

	/** Flush all cached compiled graphs. */
	void ClearCache();

private:
	TArray<FPCGGraphTask> CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId, FPCGStackContext& InOutStackContext);

	/** Compiles the top graph and applies culling optimizations if a non-unitialized grid size is provided. */
	void CompileTopGraph(UPCGGraph* InGraph, uint32 GenerationGridSize);

	/** Returns the trivial element object shared by all tasks that need it. */
	FPCGElementPtr GetSharedTrivialElement();

	/** Propagates grid sizes through a graph's compiled tasks. */
	static void ResolveGridSizes(
		EPCGHiGenGrid GenerationGrid,
		const TArray<FPCGGraphTask>& CompiledTasks,
		const FPCGStackContext& StackContext,
		EPCGHiGenGrid GenerationDefaultGrid,
		TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid);

	/** Returns the execution grid for the given task. */
	static EPCGHiGenGrid CalculateGridRecursive(
		FPCGTaskId InTaskId,
		EPCGHiGenGrid GenerationDefaultGrid,
		const FPCGStackContext& InStackContext,
		const TArray<FPCGGraphTask>& InCompiledTasks,
		TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid);

	/** Create linkage tasks for edges that cross from large grid to small grid tasks. */
	static void CreateGridLinkages(
		EPCGHiGenGrid InGenerationGrid,
		TArray<EPCGHiGenGrid>& TaskGenerationGrid,
		TArray<FPCGGraphTask>& InOutCompiledTasks,
		const FPCGStackContext& InStackContext);

	/** Discovers whether task is on a statically active branch (and needs to be passed to graph executor). */
	static bool CalculateStaticallyActiveRecursive(FPCGTaskId InTaskId, const TArray<FPCGGraphTask>& InCompiledTasks, TMap<int32, bool>& InTaskIdToActiveFlag);

	/** Culls nodes that are inactive for e.g. missing required inputs or on a statically inactive branch. */
	static void CullTasksStaticInactive(TArray<FPCGGraphTask>& InOutCompiledTasks);

	/** Culls tasks based on a given lambda. Never culls the first (input) task in the array. */
	static void CullTasks(TArray<FPCGGraphTask>& InOutCompiledTasks, bool bAddPassthroughWires, TFunctionRef<bool(const FPCGGraphTask&)> CullTask);

	/** Remove any stack frames that are not used by any task. */
	static void PostCullStackCleanup(TArray<FPCGGraphTask>& InCompiledTasks, FPCGStackContext& InOutStackContext);

	/** Task will be active if *any* of the upstream pin IDs are active, or if the pin ID list is empty. */
	static void CalculateDynamicActivePinDependencies(FPCGTaskId InTaskId, TArray<FPCGGraphTask>& InOutCompiledTasks);

	mutable FRWLock GraphToTaskMapLock;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> GraphToTaskMap;
	TMap<UPCGGraph*, FPCGStackContext> GraphToStackContext;

	// Top graphs are optimized for execution grid and store one set of compiled tasks per grid size.
	TMap<UPCGGraph*, TMap<uint32, TArray<FPCGGraphTask>>> TopGraphToTaskMap;
	TMap<UPCGGraph*, TMap<uint32, FPCGStackContext>> TopGraphToStackContextMap;

	FPCGElementPtr SharedTrivialElement;
	mutable FRWLock SharedTrivialElementLock;

#if WITH_EDITOR
	void RemoveFromCacheRecursive(UPCGGraph* InGraph);

	FCriticalSection GraphDependenciesLock;
	TMultiMap<UPCGGraph*, UPCGGraph*> GraphDependencies;
#endif // WITH_EDITOR
};
