// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "PCGElement.h"
#include "PCGGraphCache.h"
#include "PCGSubsystem.h"

#if WITH_EDITOR
#include "AsyncCompilationHelpers.h"
#include "WorldPartition/WorldPartitionHandle.h" // Needed for FWorldPartitionReference
#endif

class UPCGPin;
class UPCGGraph;
class UPCGNode;
class UPCGComponent;
class FPCGGraphCompiler;

struct FPCGGraphTaskInput
{
	FPCGGraphTaskInput(FPCGTaskId InTaskId, const UPCGPin* InInboundPin, const UPCGPin* InOutboundPin, bool bInProvideData = true)
		: TaskId(InTaskId)
		, InPin(InInboundPin)
		, OutPin(InOutboundPin)
		, bProvideData(bInProvideData)
	{
	}

	FPCGTaskId TaskId;
	const UPCGPin* InPin;
	const UPCGPin* OutPin;
	bool bProvideData;
};

struct FPCGGraphTask
{
	TArray<FPCGGraphTaskInput> Inputs;
	//TArray<DataId> Outputs;
	const UPCGNode* Node = nullptr;
	TWeakObjectPtr<UPCGComponent> SourceComponent = nullptr;
	FPCGElementPtr Element; // Added to have tasks that aren't node-bound
	FPCGContext* Context = nullptr;
	FPCGTaskId NodeId = InvalidPCGTaskId;
};

struct FPCGGraphScheduleTask
{
	TArray<FPCGGraphTask> Tasks;
};

struct FPCGGraphActiveTask
{
	FPCGElementPtr Element;
	TUniquePtr<FPCGContext> Context;
	FPCGTaskId NodeId = InvalidPCGTaskId;
#if WITH_EDITOR
	bool bIsBypassed = false;
#endif
};

class FPCGGraphExecutor
{
public:
	FPCGGraphExecutor(UObject* InOwner);
	~FPCGGraphExecutor();

	/** Compile (and cache) a graph for later use. This call is threadsafe */
	void Compile(UPCGGraph* InGraph);

	/** Schedules the execution of a given graph with specified inputs. This call is threadsafe */
	FPCGTaskId Schedule(UPCGComponent* InComponent, const TArray<FPCGTaskId>& TaskDependency = TArray<FPCGTaskId>());
	FPCGTaskId Schedule(UPCGGraph* Graph, UPCGComponent* InSourceComponent, FPCGElementPtr InputElement, const TArray<FPCGTaskId>& TaskDependency);

	// Back compatibility function. Use ScheduleGenericWithContext
	FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskDependencies);

	/** General job scheduling
	*  @param InOperation:       Callback that takes a Context as argument and returns true if the task is done, false otherwise
	*  @param InSourceComponent: PCG component associated with this task. Can be null.
	*  @param TaskDependencies:  List of all the dependencies for this task.
	*  @param bConsumeInputData: If your task need a context, but don't need the input data, set this flag to false. Default is true.
	*/
	FPCGTaskId ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskDependencies, bool bConsumeInputData = true);

	/** Gets data in the output results. Returns false if data is not ready. */
	bool GetOutputData(FPCGTaskId InTaskId, FPCGDataCollection& OutData);

#if WITH_EDITOR
	FPCGTaskId ScheduleDebugWithTaskCallback(UPCGComponent* InComponent, TFunction<void(FPCGTaskId, const UPCGNode*, const FPCGDataCollection&)> TaskCompleteCallback);

	void AddToDirtyActors(AActor* Actor);
	void AddToUnusedActors(const TSet<FWorldPartitionReference>& UnusedActors);

	/** Notify compiler that graph has changed so it'll be removed from the cache */
	void NotifyGraphChanged(UPCGGraph* InGraph);
#endif

	/** "Tick" of the graph executor. This call is NOT THREADSAFE */
	void Execute();

	/** Expose cache so it can be dirtied */
	FPCGGraphCache& GetCache() { return GraphCache; }

private:
	void QueueNextTasks(FPCGTaskId FinishedTask);
	void BuildTaskInput(const FPCGGraphTask& Task, FPCGDataCollection& TaskInput);
	void StoreResults(FPCGTaskId InTaskId, const FPCGDataCollection& InTaskOutput);
	void ClearResults();

	FPCGElementPtr GetFetchInputElement();

#if WITH_EDITOR
	void SaveDirtyActors();
	void ReleaseUnusedActors();

	void UpdateGenerationNotification(int32 RemainingTaskNum);
	void EndGenerationNotification();
	static FTextFormat GetNotificationTextFormat();
#endif

	/** Graph compiler that turns a graph into tasks */
	TUniquePtr<FPCGGraphCompiler> GraphCompiler;

	/** Rootset to hold temporary results (for current computation) + graph cache */
	FPCGRootSet DataRootSet;

	/** Graph results cache */
	FPCGGraphCache GraphCache;

	/** Input fetch element, stored here so we have only one */
	FPCGElementPtr FetchInputElement;

	FCriticalSection ScheduleLock;
	TArray<FPCGGraphScheduleTask> ScheduledTasks;

	TMap<FPCGTaskId, FPCGGraphTask> Tasks;
	TArray<FPCGGraphTask> ReadyTasks;
	TArray<FPCGGraphActiveTask> ActiveTasks;
	TArray<FPCGGraphActiveTask> SleepingTasks;
	TMap<FPCGTaskId, TSet<FPCGTaskId>> TaskSuccessors;
	/** Map of node instances to their output, could be cleared once execution is done */
	/** Note: this should at some point unload based on loaded/unloaded proxies, otherwise memory cost will be unbounded */
	TMap<FPCGTaskId, FPCGDataCollection> OutputData;
	/** Monotonically increasing id. Should be reset once all tasks are executed, should be protected by the ScheduleLock */
	FPCGTaskId NextTaskId = 0;

	/** Runtime information */
	int32 CurrentlyUsedThreads = 0;

#if WITH_EDITOR
	FCriticalSection ActorsListLock;
	TSet<AActor*> ActorsToSave;
	TSet<FWorldPartitionReference> ActorsToRelease;

	int32 CountUntilGC = 30;
	FAsyncCompilationNotification GenerationProgressNotification;
#endif
};

class FPCGFetchInputElement : public FSimplePCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough() const override { return true; }
};

class FPCGGenericElement : public FSimplePCGElement
{
public:
	FPCGGenericElement(TFunction<bool(FPCGContext*)> InOperation);
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	// Important note: generic elements must always be run on the main thread
	// as most of these will impact the editor in some way (loading, unloading, saving)
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCancellable() const override { return false; }

#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

private:
	TFunction<bool(FPCGContext*)> Operation;
};
