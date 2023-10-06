// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/PCGExtraCapture.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Graph/PCGGraphCompiler.h"
#include "Graph/PCGGraphExecutor.h"

#if WITH_EDITOR

void PCGUtils::FExtraCapture::ResetTimers()
{
	Timers.Empty();
}

void PCGUtils::FExtraCapture::ResetCapturedMessages()
{
	CapturedMessages.Empty();
}

void PCGUtils::FExtraCapture::Update(const PCGUtils::FScopedCall& InScopedCall)
{
	if (!InScopedCall.Context->Node)
	{
		return;
	}

	const double ThisFrameTime = FPlatformTime::Seconds() - InScopedCall.StartTime;

	FScopeLock ScopedLock(&Lock);

	FCallTime& Timer = Timers.FindOrAdd(InScopedCall.Context->CompiledTaskId);

	switch (InScopedCall.Phase)
	{
	case EPCGExecutionPhase::NotExecuted:
		Timer = FCallTime(); // reset it
		break;
	case EPCGExecutionPhase::PrepareData:
		Timer.PrepareDataTime = ThisFrameTime;
		break;
	case EPCGExecutionPhase::Execute:
		Timer.ExecutionTime += ThisFrameTime;
		Timer.ExecutionFrameCount++;

		Timer.MaxExecutionFrameTime = FMath::Max(Timer.MaxExecutionFrameTime, ThisFrameTime);
		Timer.MinExecutionFrameTime = FMath::Min(Timer.MinExecutionFrameTime, ThisFrameTime);
		break;
	case EPCGExecutionPhase::PostExecute:
		Timer.PostExecuteTime = ThisFrameTime;
		break;
	}

	if (!InScopedCall.CapturedMessages.IsEmpty())
	{
		TArray<FCapturedMessage>& InstanceMessages = CapturedMessages.FindOrAdd(InScopedCall.Context->Node);
		InstanceMessages.Append(std::move(InScopedCall.CapturedMessages));
	}
}

namespace PCGUtils
{
	void AddTimers(FCallTreeInfo& RootInfo, const FExtraCapture::TTimersMap& Timers, const TMap<FPCGTaskId, const FPCGGraphTask*>& TaskLookup)
	{
		// re-use this map
		TArray<FPCGTaskId> PathIds;
		TArray<const UPCGNode*> PathNodes;

		for (const auto& Pair : Timers)
		{
			const FPCGTaskId TaskId = Pair.Key;
			const FCallTime& CallTime = Pair.Value;

			PathIds.Reset();
			PathNodes.Reset();

			// build a list of parent ids that we need to populate our tree with
			PathIds.Add(TaskId);
			while (true)
			{
				const FPCGGraphTask*const * TaskItr = TaskLookup.Find(PathIds.Last());
				if (!TaskItr || !*TaskItr)
				{
					// can't find an entry in our list ignore it
					PathIds.Reset();
					PathNodes.Reset();
					break;
				}

				const FPCGGraphTask& Task = **TaskItr;

				if (PathNodes.Num() < PathIds.Num())
				{
					PathNodes.Add(Task.Node);
				}

				if (Task.ParentId == InvalidPCGTaskId)
				{
					break;
				}

				PathIds.Add(Task.ParentId);
			}

			FCallTreeInfo* CurrentInfo = &RootInfo;

			// now add the entries, the order is reversed b/c top level ones were pushed to the end
			for (int32 Idx = PathIds.Num()-1; Idx >= 0; --Idx)
			{
				const FPCGTaskId CurrentId = PathIds[Idx];

				FCallTreeInfo* FoundInfo = nullptr;
				for (FCallTreeInfo& Child : CurrentInfo->Children)
				{
					if (Child.TaskId == CurrentId)
					{
						FoundInfo = &Child;
						break;
					}
				}

				if (!FoundInfo)
				{
					// didn't find an existing entry, add one
					FCallTreeInfo& Child = CurrentInfo->Children.Emplace_GetRef();
					Child.TaskId = CurrentId;
					Child.Node = PathNodes[Idx];

					CurrentInfo = &Child;
				}
				else
				{
					CurrentInfo = FoundInfo;
				}
			}

			CurrentInfo->CallTime = CallTime;
		}
	}

	void BuildTreeInfo(FCallTreeInfo& Info)
	{
		for (FCallTreeInfo& Child : Info.Children)
		{
			BuildTreeInfo(Child);

			Info.CallTime.PrepareDataTime += Child.CallTime.PrepareDataTime;
			Info.CallTime.ExecutionTime += Child.CallTime.ExecutionTime;
			Info.CallTime.PostExecuteTime += Child.CallTime.PostExecuteTime;
			Info.CallTime.MinExecutionFrameTime = FMath::Min(Info.CallTime.MinExecutionFrameTime, Child.CallTime.MinExecutionFrameTime);
			Info.CallTime.MaxExecutionFrameTime = FMath::Max(Info.CallTime.MaxExecutionFrameTime, Child.CallTime.MaxExecutionFrameTime);
		}
	}
}

PCGUtils::FCallTreeInfo PCGUtils::FExtraCapture::CalculateCallTreeInfo(const UPCGComponent* Component) const
{
	UPCGSubsystem* PCGSubsystem = Component ? Component->GetSubsystem() : nullptr;

	if (!PCGSubsystem)
	{
		return {};
	}

	const FPCGGraphCompiler* Compiler = PCGSubsystem->GetGraphCompiler();
	if (!Compiler)
	{
		return {};
	}

	FPCGStackContext DummyStackContext;
	TArray<FPCGGraphTask> CompiledTasks = Compiler->GetPrecompiledTasks(Component->GetGraph(), DummyStackContext);
	if (CompiledTasks.IsEmpty())
	{
		return {};
	}

	// the last task on a top level graph is post execute task that doesn't need to be in the call tree
	CompiledTasks.Pop(); 

	TMap<FPCGTaskId, const FPCGGraphTask*> TaskLookup;

	// build some lookup maps
	for (const FPCGGraphTask& Task : CompiledTasks)
	{
		TaskLookup.Add(Task.CompiledTaskId, &Task);
	}

	FCallTreeInfo RootInfo;

	PCGUtils::AddTimers(RootInfo, Timers, TaskLookup);
	PCGUtils::BuildTreeInfo(RootInfo);

	return RootInfo;
}

PCGUtils::FScopedCall::FScopedCall(const IPCGElement& InOwner, FPCGContext* InContext)
	: Owner(InOwner)
	, Context(InContext)
	, Phase(InContext->CurrentPhase)
	, ThreadID(FPlatformTLS::GetCurrentThreadId())
{
	StartTime = FPlatformTime::Seconds();

	GLog->AddOutputDevice(this);
}

PCGUtils::FScopedCall::~FScopedCall()
{
	GLog->RemoveOutputDevice(this);
	if (Context && Context->SourceComponent.IsValid())
	{
		Context->SourceComponent->ExtraCapture.Update(*this);
	}
}

void PCGUtils::FScopedCall::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	// TODO: this thread id check will also filter out messages spawned from threads spawned inside of nodes. To improve that,
	// perhaps set at TLS bit on things from here and inside of PCGAsync spawned jobs. If this was done, CapturedMessages below also will
	// need protection
	if (Verbosity > ELogVerbosity::Warning || FPlatformTLS::GetCurrentThreadId() != ThreadID)
	{
		// ignore
		return;
	}

	// this is a dumb counter just so messages can be sorted in a similar order as when they were logged
	static volatile int32 MessageCounter = 0;

	CapturedMessages.Add(PCGUtils::FCapturedMessage{ MessageCounter++, Category, V, Verbosity });
}

#endif // WITH_EDITOR