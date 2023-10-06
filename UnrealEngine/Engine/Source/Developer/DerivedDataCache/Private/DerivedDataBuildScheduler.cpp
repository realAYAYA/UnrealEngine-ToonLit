// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildScheduler.h"
#include "DerivedDataBuildFunction.h"
#include "DerivedDataBuildFunctionFactory.h"
#include "DerivedDataBuildFunctionRegistry.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataBuildSchedulerQueue.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataThreadPoolTask.h"
#include "Experimental/Misc/ExecutionResource.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeRWLock.h"

namespace UE::DerivedData::Private
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildSchedulerTypeQueue
{
public:
	FBuildSchedulerTypeQueue();
	~FBuildSchedulerTypeQueue();

	void Queue(const FUtf8SharedString& TypeName, IRequestOwner& Owner, TUniqueFunction<void ()>&& OnComplete);

private:
	void OnModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature);
	void OnModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature);

	void AddQueueNoLock(IBuildSchedulerTypeQueue* Queue);
	void RemoveQueue(IBuildSchedulerTypeQueue* Queue);

private:
	mutable FRWLock Lock;
	TMap<FUtf8SharedString, IBuildSchedulerTypeQueue*> Queues;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildSchedulerTypeQueue::FBuildSchedulerTypeQueue()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	for (IBuildSchedulerTypeQueue* Queue : ModularFeatures.GetModularFeatureImplementations<IBuildSchedulerTypeQueue>(IBuildSchedulerTypeQueue::FeatureName))
	{
		AddQueueNoLock(Queue);
	}
	ModularFeatures.OnModularFeatureRegistered().AddRaw(this, &FBuildSchedulerTypeQueue::OnModularFeatureRegistered);
	ModularFeatures.OnModularFeatureUnregistered().AddRaw(this, &FBuildSchedulerTypeQueue::OnModularFeatureUnregistered);
}

FBuildSchedulerTypeQueue::~FBuildSchedulerTypeQueue()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ModularFeatures.OnModularFeatureUnregistered().RemoveAll(this);
	ModularFeatures.OnModularFeatureRegistered().RemoveAll(this);
}

void FBuildSchedulerTypeQueue::OnModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (Type == IBuildSchedulerTypeQueue::FeatureName)
	{
		FWriteScopeLock WriteLock(Lock);
		AddQueueNoLock(static_cast<IBuildSchedulerTypeQueue*>(ModularFeature));
	}
}

void FBuildSchedulerTypeQueue::OnModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (Type == IBuildSchedulerTypeQueue::FeatureName)
	{
		RemoveQueue(static_cast<IBuildSchedulerTypeQueue*>(ModularFeature));
	}
}

void FBuildSchedulerTypeQueue::AddQueueNoLock(IBuildSchedulerTypeQueue* Queue)
{
	const FUtf8SharedString& TypeName = Queue->GetTypeName();
	const uint32 TypeNameHash = GetTypeHash(TypeName);
	if (TypeName.IsEmpty())
	{
		UE_LOG(LogDerivedDataBuild, Error,
			TEXT("An empty type name is not allowed in a build scheduler type queue."));
	}
	else if (Queues.FindByHash(TypeNameHash, TypeName))
	{
		UE_LOG(LogDerivedDataBuild, Error,
			TEXT("More than one build scheduler type queue has been registered with the type name %s."),
			*WriteToString<64>(TypeName));
	}
	else
	{
		Queues.EmplaceByHash(TypeNameHash, TypeName, Queue);
	}
}

void FBuildSchedulerTypeQueue::RemoveQueue(IBuildSchedulerTypeQueue* Queue)
{
	const FUtf8SharedString& TypeName = Queue->GetTypeName();
	const uint32 TypeNameHash = GetTypeHash(TypeName);
	FWriteScopeLock WriteLock(Lock);
	Queues.RemoveByHash(TypeNameHash, TypeName);
}

void FBuildSchedulerTypeQueue::Queue(
	const FUtf8SharedString& TypeName,
	IRequestOwner& Owner,
	TUniqueFunction<void ()>&& OnComplete)
{
	if (FReadScopeLock ReadLock(Lock); IBuildSchedulerTypeQueue* Queue = Queues.FindRef(TypeName))
	{
		return Queue->Queue(Owner, MoveTemp(OnComplete));
	}

	OnComplete();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildSchedulerMemoryQueue
{
public:
	FBuildSchedulerMemoryQueue();
	~FBuildSchedulerMemoryQueue();

	void Reserve(uint64 Memory, IRequestOwner& Owner, TUniqueFunction<void ()>&& OnComplete);

private:
	void OnModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature);
	void OnModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature);

	void AddQueueNoLock(IBuildSchedulerMemoryQueue* Queue);
	void RemoveQueue(IBuildSchedulerMemoryQueue* Queue);

private:
	mutable FRWLock Lock;
	IBuildSchedulerMemoryQueue* GlobalQueue = nullptr;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildSchedulerMemoryQueue::FBuildSchedulerMemoryQueue()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	for (IBuildSchedulerMemoryQueue* Queue : ModularFeatures.GetModularFeatureImplementations<IBuildSchedulerMemoryQueue>(IBuildSchedulerMemoryQueue::FeatureName))
	{
		AddQueueNoLock(Queue);
	}
	ModularFeatures.OnModularFeatureRegistered().AddRaw(this, &FBuildSchedulerMemoryQueue::OnModularFeatureRegistered);
	ModularFeatures.OnModularFeatureUnregistered().AddRaw(this, &FBuildSchedulerMemoryQueue::OnModularFeatureUnregistered);
}

FBuildSchedulerMemoryQueue::~FBuildSchedulerMemoryQueue()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ModularFeatures.OnModularFeatureUnregistered().RemoveAll(this);
	ModularFeatures.OnModularFeatureRegistered().RemoveAll(this);
}

void FBuildSchedulerMemoryQueue::OnModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (Type == IBuildSchedulerMemoryQueue::FeatureName)
	{
		FWriteScopeLock WriteLock(Lock);
		AddQueueNoLock(static_cast<IBuildSchedulerMemoryQueue*>(ModularFeature));
	}
}

void FBuildSchedulerMemoryQueue::OnModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (Type == IBuildSchedulerMemoryQueue::FeatureName)
	{
		RemoveQueue(static_cast<IBuildSchedulerMemoryQueue*>(ModularFeature));
	}
}

void FBuildSchedulerMemoryQueue::AddQueueNoLock(IBuildSchedulerMemoryQueue* Queue)
{
	if (GlobalQueue)
	{
		UE_LOG(LogDerivedDataBuild, Error, TEXT("More than one build scheduler memory queue has been registered."));
	}
	else
	{
		GlobalQueue = Queue;
	}
}

void FBuildSchedulerMemoryQueue::RemoveQueue(IBuildSchedulerMemoryQueue* Queue)
{
	FWriteScopeLock WriteLock(Lock);
	if (GlobalQueue == Queue)
	{
		GlobalQueue = nullptr;
	}
}

void FBuildSchedulerMemoryQueue::Reserve(const uint64 Memory, IRequestOwner& Owner, TUniqueFunction<void ()>&& OnComplete)
{
	if (FReadScopeLock ReadLock(Lock); IBuildSchedulerMemoryQueue* Queue = GlobalQueue)
	{
		return Queue->Reserve(Memory, Owner, MoveTemp(OnComplete));
	}

	OnComplete();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildJobSchedule final : public IBuildJobSchedule
{
public:
	FBuildJobSchedule(
		IBuildJob& InJob,
		IRequestOwner& InOwner,
		FBuildSchedulerTypeQueue& InTypeQueue,
		FBuildSchedulerMemoryQueue& InMemoryQueue)
		: Job(InJob)
		, Owner(InOwner)
		, TypeQueue(InTypeQueue)
		, MemoryQueue(InMemoryQueue)
	{
	}

	FBuildSchedulerParams& EditParameters() final { return Params; }

	void ScheduleCacheQuery() final       { QueueForType([this] { StepSync(); }); }
	void ScheduleCacheStore() final       { StepSync(); }

	void ScheduleResolveKey() final       { StepAsync(TEXT("ResolveKey")); }
	void ScheduleResolveInputMeta() final { StepAsync(TEXT("ResolveInputMeta")); }

	void ScheduleResolveInputData() final
	{
		if (Params.MissingRemoteInputsSize)
		{
			QueueForType([this] { StepAsync(TEXT("ResolveInputData")); });
		}
		else
		{
			QueueForTypeThenMemory([this] { StepAsync(TEXT("ResolveInputData")); });
		}
	}

	void ScheduleExecuteRemote() final    { QueueForType([this] { StepAsync(TEXT("ExecuteRemote")); }); }
	void ScheduleExecuteLocal() final     { QueueForTypeThenMemory([this] { StepAsync(TEXT("ExecuteLocal")); }); }

	void EndJob() final
	{
		MemoryExecutionResources = nullptr;
	}

private:
	void StepSync()
	{
		Job.StepExecution();
		// DO NOT ACCESS THIS AGAIN PAST THIS POINT!
	}

	void StepAsync(const TCHAR* DebugName)
	{
		if (Owner.GetPriority() == EPriority::Blocking)
		{
			StepSync();
		}
		else
		{
			Owner.LaunchTask(DebugName, [this] { Job.StepExecution(); });
		}
		// DO NOT ACCESS THIS AGAIN PAST THIS POINT!
	}

	void QueueForType(TUniqueFunction<void ()>&& OnComplete)
	{
		if (bQueued)
		{
			return OnComplete();
		}

		bQueued = true;
		TypeQueue.Queue(Params.TypeName, Owner, [this, OnComplete = MoveTemp(OnComplete)]
		{ 
			TypeExecutionResources = FExecutionResourceContext::Get();
			OnComplete();
		});
		// DO NOT ACCESS THIS AGAIN PAST THIS POINT!
	}

	void QueueForMemory(TUniqueFunction<void ()>&& OnComplete)
	{
		check(Params.TotalRequiredMemory >= Params.ResolvedInputsSize);
		const uint64 CurrentMemory = Params.TotalRequiredMemory - Params.ResolvedInputsSize;

		// Reserve memory only once, the first time it is needed for local execution.
		// This will occur either when resolving input data for local execution or before beginning local execution.
		// NOTE: No attempt is made to reserve memory prior to remote execution, which may become a problem if remote
		//       execution frequently requires input data to be loaded.
		if (ReservedMemory || CurrentMemory == 0)
		{
			return OnComplete();
		}

		ReservedMemory = CurrentMemory;
		MemoryQueue.Reserve(CurrentMemory, Owner, [this, OnComplete = MoveTemp(OnComplete)]
		{
			MemoryExecutionResources = FExecutionResourceContext::Get();
			OnComplete();
		});
		// DO NOT ACCESS THIS AGAIN PAST THIS POINT!
	}

	void QueueForTypeThenMemory(TUniqueFunction<void ()>&& OnComplete)
	{
		QueueForType([this, OnComplete = MoveTemp(OnComplete)]() mutable { QueueForMemory(MoveTemp(OnComplete)); });
	}

private:
	IBuildJob& Job;
	IRequestOwner& Owner;
	FBuildSchedulerParams Params;
	FBuildSchedulerTypeQueue& TypeQueue;
	FBuildSchedulerMemoryQueue& MemoryQueue;
	TRefCountPtr<IExecutionResource> TypeExecutionResources;
	TRefCountPtr<IExecutionResource> MemoryExecutionResources;
	uint64 ReservedMemory = 0;
	bool bQueued = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildScheduler final : public IBuildScheduler
{
	TUniquePtr<IBuildJobSchedule> BeginJob(IBuildJob& Job, IRequestOwner& Owner) final
	{
		return MakeUnique<FBuildJobSchedule>(Job, Owner, TypeQueue, MemoryQueue);
	}

	FBuildSchedulerTypeQueue TypeQueue;
	FBuildSchedulerMemoryQueue MemoryQueue;
};

IBuildScheduler* CreateBuildScheduler()
{
	return new FBuildScheduler();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // UE::DerivedData::Private
