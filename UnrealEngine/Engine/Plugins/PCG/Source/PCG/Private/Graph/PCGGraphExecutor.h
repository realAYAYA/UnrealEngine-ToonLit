// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGSubsystem.h"
#include "Graph/PCGGraphCache.h"
#include "Graph/PCGGraphCompiler.h"
#include "Graph/PCGPinDependencyExpression.h"
#include "Graph/PCGStackContext.h"

#include "UObject/GCObject.h"

#if WITH_EDITOR
#include "AsyncCompilationHelpers.h"
#include "WorldPartition/WorldPartitionHandle.h" // Needed for FWorldPartitionReference
#endif

class UPCGPin;
class UPCGGraph;
class UPCGNode;
class UPCGComponent;
class FPCGGraphCompiler;
struct FPCGStack;
class FPCGStackContext;
class FTextFormat;

namespace PCGGraphExecutor
{
	extern PCG_API TAutoConsoleVariable<float> CVarTimePerFrame;
	extern PCG_API TAutoConsoleVariable<bool> CVarGraphMultithreading;

#if WITH_EDITOR
	extern PCG_API TAutoConsoleVariable<float> CVarEditorTimePerFrame;
#endif
}

struct FPCGGraphTaskInput
{
	FPCGGraphTaskInput(FPCGTaskId InTaskId, const UPCGPin* InInboundPin, const UPCGPin* InOutboundPin, bool bInProvideData = true)
		: TaskId(InTaskId)
		, InPin(InInboundPin)
		, OutPin(InOutboundPin)
		, bProvideData(bInProvideData)
	{
	}

#if WITH_EDITOR
	bool operator==(const FPCGGraphTaskInput& Other) const;
#endif

	FPCGTaskId TaskId;

	/** The upstream output pin from which the input data comes. */
	const UPCGPin* InPin;

	/** The input pin on the task element. */
	const UPCGPin* OutPin;

	/** Whether the input provides any data. For the post execute task, only the output node will provide data. */
	bool bProvideData;
};

struct FPCGGraphTask
{
#if WITH_EDITOR
	/** Approximate equivalence. Does not deeply check node settings, nor does it do a deep comparison of the element. */
	bool IsApproximatelyEqual(const FPCGGraphTask& Other) const;

	/** Because we might not already have a context, but still want to attach some logs to the node, use this utility function */
	void LogVisual(ELogVerbosity::Type InVerbosity, const FText& InMessage) const;
#endif

	const FPCGStack* GetStack() const;

	TArray<FPCGGraphTaskInput> Inputs;
	const UPCGNode* Node = nullptr;
	TWeakObjectPtr<UPCGComponent> SourceComponent = nullptr;
	FPCGElementPtr Element; // Added to have tasks that aren't node-bound
	FPCGContext* Context = nullptr;
	FPCGTaskId NodeId = InvalidPCGTaskId;
	FPCGTaskId CompiledTaskId = InvalidPCGTaskId; // the task id as it exists when compiled
	FPCGTaskId ParentId = InvalidPCGTaskId; // represents the parent sub object graph task, if we were called from one

	/** Conjunction of disjunctions of pin IDs that are required to be active for this task to be active.
	* Example - keep task if: UpstreamPin0Active && (UpstreamPin1Active || UpstreamPin2Active)
	*/
	FPCGPinDependencyExpression PinDependency;

	int32 StackIndex = INDEX_NONE;
	TSharedPtr<const FPCGStackContext> StackContext;
};

struct FPCGGraphScheduleTask
{
	TArray<FPCGGraphTask> Tasks;
	TWeakObjectPtr<UPCGComponent> SourceComponent = nullptr;
	int32 FirstTaskIndex = 0;
	int32 LastTaskIndex = 0;
	bool bHasAbortCallbacks = false;
};

struct FPCGGraphActiveTask
{
	TArray<FPCGGraphTaskInput> Inputs;
	FPCGElementPtr Element;
	TUniquePtr<FPCGContext> Context;
	FPCGTaskId NodeId = InvalidPCGTaskId;
	bool bWasCancelled = false;
#if WITH_EDITOR
	bool bIsBypassed = false;
#endif
	int32 StackIndex = INDEX_NONE;
	TSharedPtr<const FPCGStackContext> StackContext;
};

class FPCGGraphExecutor : public FGCObject
{
public:
	FPCGGraphExecutor();
	~FPCGGraphExecutor();

	/** Compile (and cache) a graph for later use. This call is threadsafe */
	void Compile(UPCGGraph* InGraph);

	/** Schedules the execution of a given graph with specified inputs. This call is threadsafe */
	FPCGTaskId Schedule(UPCGComponent* InComponent, const TArray<FPCGTaskId>& TaskDependency = TArray<FPCGTaskId>(), const FPCGStack* InFromStack = nullptr);

	/** Schedules the execution of a given graph with specified inputs. This call is threadsafe */
	FPCGTaskId Schedule(
		UPCGGraph* Graph,
		UPCGComponent* InSourceComponent,
		FPCGElementPtr PreGraphElement,
		FPCGElementPtr InputElement,
		const TArray<FPCGTaskId>& TaskDependency,
		const FPCGStack* InFromStack,
		bool bAllowHierarchicalGeneration);

	/** Cancels all tasks originating from the given component */
	TArray<UPCGComponent*> Cancel(UPCGComponent* InComponent);
	
	/** Cancels all tasks running a given graph */
	TArray<UPCGComponent*> Cancel(UPCGGraph* InGraph);

	/** Cancels all tasks */
	TArray<UPCGComponent*> CancelAll();

	/** Returns true if any task is scheduled or executing for the given graph. */
	bool IsGraphCurrentlyExecuting(UPCGGraph* InGraph);

	/** Returns true if any task is scheduled or executing for any graph */
	bool IsAnyGraphCurrentlyExecuting() const;
	
	// Back compatibility function. Use ScheduleGenericWithContext
	FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies);
	FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, TFunction<void()> InAbortOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies);

	/** General job scheduling
	*  @param InOperation:               Callback that takes a Context as argument and returns true if the task is done, false otherwise
	*  @param InSourceComponent:         PCG component associated with this task. Can be null.
	*  @param TaskExecutionDependencies: Task will wait on these tasks to execute and won't take their output data as input.
	*  @param TaskDataDependencies:      Task will wait on these tasks to execute and will take their output data as input.
	*/
	FPCGTaskId ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies);

	/** General job scheduling
	*  @param InOperation:               Callback that takes a Context as argument and returns true if the task is done, false otherwise
	*  @param InAbortOperation:          Callback that is called if the task is aborted (cancelled) before fully executed.
	*  @param InSourceComponent:         PCG component associated with this task. Can be null.
	*  @param TaskExecutionDependencies: Task will wait on these tasks to execute and won't take their output data as input.
	*  @param TaskDataDependencies:      Task will wait on these tasks to execute and will take their output data as input.
	*/
	FPCGTaskId ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, TFunction<void(FPCGContext*)> InAbortOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies);

	/** Gets data in the output results. Returns false if data is not ready. */
	bool GetOutputData(FPCGTaskId InTaskId, FPCGDataCollection& OutData);

	/** Accessor so PCG tools (e.g. profiler) can easily decode graph task ids **/
	FPCGGraphCompiler& GetCompiler() { return GraphCompiler; }

	/** Accessor so PCG tools (e.g. profiler) can easily decode graph task ids **/
	const FPCGGraphCompiler& GetCompiler() const { return GraphCompiler; }

#if WITH_EDITOR
	FPCGTaskId ScheduleDebugWithTaskCallback(UPCGComponent* InComponent, TFunction<void(FPCGTaskId, const UPCGNode*, const FPCGDataCollection&)> TaskCompleteCallback);

	void AddToDirtyActors(AActor* Actor);
	void AddToUnusedActors(const TSet<FWorldPartitionReference>& UnusedActors);

	/** Notify compiler that graph has changed so it'll be removed from the cache */
	void NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType);

	/** Returns the number of entries currently in the cache for InElement. */
	uint32 GetGraphCacheEntryCount(IPCGElement* InElement) const { return GraphCache.GetGraphCacheEntryCount(InElement); }
#endif

	/** "Tick" of the graph executor. This call is NOT THREADSAFE */
	void Execute();

	/** Expose cache so it can be dirtied */
	FPCGGraphCache& GetCache() { return GraphCache; }

	/** True if graph cache debugging is enabled. */
	bool IsGraphCacheDebuggingEnabled() const { return GraphCache.IsDebuggingEnabled(); }

	//~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FPCGGraphExecutor"); }
	//~End FGCObject interface

private:
	TSet<UPCGComponent*> Cancel(TFunctionRef<bool(TWeakObjectPtr<UPCGComponent>)> CancelFilter);
	void ClearAllTasks();
	void QueueNextTasks(FPCGTaskId FinishedTask, bool bIgnoreMissingTasks = false);
	bool CancelNextTasks(FPCGTaskId CancelledTask, TSet<UPCGComponent*>& OutCancelledComponents);
	void RemoveTaskFromInputSuccessors(FPCGTaskId CancelledTask, const TArray<FPCGGraphTaskInput>& CancelledTaskInputs);
	void BuildTaskInput(const FPCGGraphTask& Task, FPCGDataCollection& TaskInput);
	/** Combine all param data into one on the Params pin, if any.*/
	void CombineParams(FPCGTaskId InTaskId, FPCGDataCollection& InTaskInput);
	void StoreResults(FPCGTaskId InTaskId, const FPCGDataCollection& InTaskOutput);
	void ClearResults();

	/** If the completed task has one or more deactivated pins, delete any downstream tasks that are inactive as a result. */
	void CullInactiveDownstreamNodes(FPCGTaskId CompletedTaskId, uint64 InInactiveOutputPinBitmask);

	/** Builds an array of all deactivated unique pin IDs. */
	void GetPinIdsToDeactivate(FPCGTaskId TaskId, uint64 InactiveOutputPinBitmask, TArray<FPCGPinId>& InOutPinIds);

	FPCGElementPtr GetFetchInputElement();

	void LogTaskState() const;

	int32 GetNonScheduledRemainingTaskCount() const;

#if WITH_EDITOR
	/** Notify the component that the given pins were deactivated during execution. */
	void SendInactivePinNotification(const UPCGNode* InNode, const FPCGStack* InStack, uint64 InactiveOutputPinBitmask);

	void SaveDirtyActors();
	void ReleaseUnusedActors();

	void UpdateGenerationNotification();
	static FTextFormat GetNotificationTextFormat();
#endif

	/** Graph compiler that turns a graph into tasks */
	FPCGGraphCompiler GraphCompiler;

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

	int32 ReleaseActorsCountUntilGC = 30;
	FAsyncCompilationNotification GenerationProgressNotification;

	int32 TidyCacheCountUntilGC = 100;
#endif
};

class FPCGFetchInputElement : public IPCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return true; }
};

class FPCGGenericElement : public IPCGElement
{
public:
	using FContextAllocator = TFunction<FPCGContext*(const FPCGDataCollection&, TWeakObjectPtr<UPCGComponent>, const UPCGNode*)>;

	FPCGGenericElement(
		TFunction<bool(FPCGContext*)> InOperation,
		const FContextAllocator& InContextAllocator = (FContextAllocator)[](const FPCGDataCollection&, TWeakObjectPtr<UPCGComponent>, const UPCGNode*)
	{
		return new FPCGContext();
	});

	FPCGGenericElement(
		TFunction<bool(FPCGContext*)> InOperation,
		TFunction<void(FPCGContext*)> InAbortOperation,
		const FContextAllocator& InContextAllocator = (FContextAllocator)[](const FPCGDataCollection&, TWeakObjectPtr<UPCGComponent>, const UPCGNode*)
	{
		return new FPCGContext();
	});
	
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	// Important note: generic elements must always be run on the main thread
	// as most of these will impact the editor in some way (loading, unloading, saving)
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual void AbortInternal(FPCGContext* Context) const override;
	virtual bool IsCancellable() const override { return false; }

#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

private:
	TFunction<bool(FPCGContext*)> Operation;
	TFunction<void(FPCGContext*)> AbortOperation;

	/** Creates a context object for this element. */
	FContextAllocator ContextAllocator;
};

/** Context for linkage element which marshalls data across hierarchical generation grids. */
struct FPCGGridLinkageContext : public FPCGContext
{
	/** If we require data from a component that is not generated, we schedule it once to see if we can get the data later. */
	bool bScheduledGraph = false;
};

namespace PCGGraphExecutor
{
	/** Marshals data across grid sizes at execution time. */
	class FPCGGridLinkageElement : public FPCGGenericElement
	{
	public:
		FPCGGridLinkageElement(TFunction<bool(FPCGContext*)> InOperation, const FContextAllocator& InContextAllocator, EPCGHiGenGrid InFromGrid, EPCGHiGenGrid InToGrid, const FString& InResourceKey)
			: FPCGGenericElement(InOperation, InContextAllocator)
			, FromGrid(InFromGrid)
			, ToGrid(InToGrid)
			, ResourceKey(InResourceKey)
		{
		}

#if WITH_EDITOR
		//~Begin IPCGElement interface
		virtual bool IsGridLinkage() const override { return true; }
		//~End IPCGElement interface

		/** Return true if the grid sizes & path match. */
		bool operator==(const FPCGGridLinkageElement& Other) const;
#endif

	private:
		// These values are stored here so that we can compare two grid linkage elements for equivalence.
		EPCGHiGenGrid FromGrid = EPCGHiGenGrid::Uninitialized;
		EPCGHiGenGrid ToGrid = EPCGHiGenGrid::Uninitialized;
		FString ResourceKey;
	};

	/** Compares InFromGrid and InToGrid and performs data storage/retrieval as necessary to marshal data across execution grids. */
	bool ExecuteGridLinkage(
		EPCGHiGenGrid InGenerationGrid,
		EPCGHiGenGrid InFromGrid,
		EPCGHiGenGrid InToGrid,
		const FString& InResourceKey,
		const FName& InOutputPinLabel,
		const UPCGNode* InDownstreamNode,
		FPCGGridLinkageContext* InContext);
}
