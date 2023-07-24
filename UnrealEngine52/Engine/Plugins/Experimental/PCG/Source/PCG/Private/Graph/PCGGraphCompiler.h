// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"

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
	TArray<FPCGGraphTask> GetCompiledTasks(UPCGGraph* InGraph, bool bIsTopGraph = true);
	TArray<FPCGGraphTask> GetPrecompiledTasks(UPCGGraph* InGraph, bool bIsTopGraph = true) const;

	static void OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset, FPCGTaskId ParentId);

#if WITH_EDITOR
	void NotifyGraphChanged(UPCGGraph* InGraph);
#endif

private:
	TArray<FPCGGraphTask> CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId);
	void CompileTopGraph(UPCGGraph* InGraph);

	mutable FRWLock GraphToTaskMapLock;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> GraphToTaskMap;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> TopGraphToTaskMap;

#if WITH_EDITOR
	void RemoveFromCache(UPCGGraph* InGraph);
	void RemoveFromCacheRecursive(UPCGGraph* InGraph);

	FCriticalSection GraphDependenciesLock;
	TMultiMap<UPCGGraph*, UPCGGraph*> GraphDependencies;
#endif // WITH_EDITOR
};
