// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "Graph/PCGStackContext.h"

class UPCGGraph;
struct FPCGGraphTask;

/** 
* FPCGGraphCompiler
* This class compiles a graph into tasks and keeps an internal cache
*/
class FPCGGraphCompiler
{
public:
	void Compile(UPCGGraph* InGraph);
	TArray<FPCGGraphTask> GetCompiledTasks(UPCGGraph* InGraph, FPCGStackContext& OutStackContext, bool bIsTopGraph = true);
	TArray<FPCGGraphTask> GetPrecompiledTasks(UPCGGraph* InGraph, FPCGStackContext& OutStackContext, bool bIsTopGraph = true) const;

	static void OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset, FPCGTaskId ParentId);

	/** Propagates grid sizes through a graph's compiled tasks. */
	void ResolveGridSizes(TArray<FPCGGraphTask>& InOutCompiledTasks, const FPCGStackContext& InStackContext) const;

#if WITH_EDITOR
	void NotifyGraphChanged(UPCGGraph* InGraph);
#endif

private:
	TArray<FPCGGraphTask> CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId, FPCGStackContext& InOutStackContext);
	void CompileTopGraph(UPCGGraph* InGraph);

	/** Returns the grid for InCompiledTaskId which will be a concrete grid size or GenerationDefault if it cannot be determined statically. */
	EPCGHiGenGrid CalculateGridRecursive(FPCGTaskId InTaskId, const FPCGStackContext& InStackContext, TArray<FPCGGraphTask>& InOutCompiledTasks) const;

	mutable FRWLock GraphToTaskMapLock;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> GraphToTaskMap;
	TMap<UPCGGraph*, FPCGStackContext> GraphToStackContext;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> TopGraphToTaskMap;
	TMap<UPCGGraph*, FPCGStackContext> TopGraphToStackContext;

#if WITH_EDITOR
	void RemoveFromCache(UPCGGraph* InGraph);
	void RemoveFromCacheRecursive(UPCGGraph* InGraph);

	FCriticalSection GraphDependenciesLock;
	TMultiMap<UPCGGraph*, UPCGGraph*> GraphDependencies;
#endif // WITH_EDITOR
};
