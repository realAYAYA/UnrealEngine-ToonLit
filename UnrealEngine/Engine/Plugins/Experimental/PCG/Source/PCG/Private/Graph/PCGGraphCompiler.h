// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGGraphExecutor.h"

/** 
* FPCGGraphCompiler
* This class compiles a graph into tasks and keeps an internal cache
*/
class FPCGGraphCompiler
{
public:
	void Compile(UPCGGraph* InGraph);
	TArray<FPCGGraphTask> GetCompiledTasks(UPCGGraph* InGraph, bool bIsTopGraph = true);

	static void OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset);

#if WITH_EDITOR
	void NotifyGraphChanged(UPCGGraph* InGraph);
#endif

private:
	TArray<FPCGGraphTask> CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId);
	void CompileTopGraph(UPCGGraph* InGraph);

	FRWLock GraphToTaskMapLock;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> GraphToTaskMap;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> TopGraphToTaskMap;

#if WITH_EDITOR
	void RemoveFromCache(UPCGGraph* InGraph);
	void RemoveFromCacheRecursive(UPCGGraph* InGraph);

	FCriticalSection GraphDependenciesLock;
	TMultiMap<UPCGGraph*, UPCGGraph*> GraphDependencies;
#endif // WITH_EDITOR
};
