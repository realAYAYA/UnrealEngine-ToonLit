// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphExecutionLogging.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "Graph/PCGGraphExecutor.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

namespace PCGGraphExecutionLogging
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	static TAutoConsoleVariable<bool> CVarGraphExecutionLoggingEnable(
		TEXT("pcg.GraphExecution.EnableLogging"),
		false,
		TEXT("Enables fine grained log of graph execution"));

	static TAutoConsoleVariable<bool> CVarGraphExecutionCullingLoggingEnable(
		TEXT("pcg.GraphExecution.EnableCullingLogging"),
		false,
		TEXT("Enables fine grained log of dynamic task culling during graph execution"));
#endif

	bool LogEnabled()
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		return CVarGraphExecutionLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

	bool CullingLogEnabled()
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		return CVarGraphExecutionCullingLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	FString GetOwnerName(const UPCGComponent* InComponent)
	{
		return (InComponent && InComponent->GetOwner()) ?
#if WITH_EDITOR
			InComponent->GetOwner()->GetActorLabel() :
#else
			InComponent->GetOwner()->GetName() :
#endif
			FString(TEXT("MISSINGOWNER"));
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING

	void LogGraphTask(FPCGTaskId TaskId, const FPCGGraphTask& Task, const TSet<FPCGTaskId>* SuccessorIds)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		auto GenerateInputsString = [](const TArray<FPCGGraphTaskInput>& Inputs)
		{
			FString InputString;
			bool bFirstInput = true;

			for (const FPCGGraphTaskInput& Input : Inputs)
			{
				if (!bFirstInput)
				{
					InputString += TEXT(",");
				}
				bFirstInput = false;

				InputString += FString::Printf(TEXT("%u->'%s'"), Input.TaskId, Input.OutPin ? *Input.OutPin->Properties.Label.ToString() : TEXT(""));
			}

			return InputString;
		};

		FString SuccessorsString;
		if (SuccessorIds)
		{
			bool bFirstSuccessor = true;
			for (const FPCGTaskId& SuccessorId : *SuccessorIds)
			{
				SuccessorsString += bFirstSuccessor ? FString::Printf(TEXT("%u"), SuccessorId) : FString::Printf(TEXT(",%u"), SuccessorId);
				bFirstSuccessor = false;
			}
		}

		const FString PinDependencyString =
#if WITH_EDITOR
			Task.PinDependency.ToString();
#else
			TEXT("MISSINGPINDEPS");
#endif

		UE_LOG(LogPCG, Log, TEXT("\t\tID: %u\tParent: %u\tNode: %s\tInputs: %s\tPinDeps: %s\tSuccessors: %s"),
			TaskId,
			Task.ParentId != InvalidPCGTaskId ? Task.ParentId : 0,
			Task.Node ? (*Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString()) : TEXT("NULL"),
			*GenerateInputsString(Task.Inputs),
			*PinDependencyString,
			*SuccessorsString
		);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphTasks(const TMap<FPCGTaskId, FPCGGraphTask>& Tasks, const TMap<FPCGTaskId, TSet<FPCGTaskId>>* TaskSuccessors)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		for (const TPair<FPCGTaskId, FPCGGraphTask>& TaskIdAndTask : Tasks)
		{
			PCGGraphExecutionLogging::LogGraphTask(TaskIdAndTask.Key, TaskIdAndTask.Value, TaskSuccessors ? TaskSuccessors->Find(TaskIdAndTask.Value.NodeId) : nullptr);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphTasks(const TArray<FPCGGraphTask>& Tasks)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		for (const FPCGGraphTask& Task : Tasks)
		{
			LogGraphTask(Task.NodeId, Task);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphSchedule(const UPCGComponent* SourceComponent, const UPCGGraph* InScheduledGraph)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		UE_LOG(LogPCG, Display, TEXT("[%s/%s] --- SCHEDULE GRAPH %s ---"),
			(SourceComponent && SourceComponent->GetOwner()) ? *SourceComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(SourceComponent && SourceComponent->GetGraph()) ? *SourceComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			InScheduledGraph ? *InScheduledGraph->GetName() : TEXT("MISSINGGRAPH)"));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphScheduleDependency(const UPCGComponent* InComponent, const FPCGStack* InFromStack)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		FString FromStackString;
		if (InFromStack)
		{
			InFromStack->CreateStackFramePath(FromStackString);
		}

		UE_LOG(LogPCG, Display, TEXT("[%s/%s] --- SCHEDULE GRAPH FOR DEPENDENCY, from stack: %s"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			*FromStackString);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphScheduleDependencyFailed(const UPCGComponent* InComponent, const FPCGStack* InFromStack)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		FString FromStackString;
		if (InFromStack)
		{
			InFromStack->CreateStackFramePath(FromStackString);
		}

		UE_LOG(LogPCG, Warning, TEXT("[%s/%s] Failed to schedule dependency, from stack: %s"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			*FromStackString);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}
	
	void LogGraphPostSchedule(const TMap<FPCGTaskId, FPCGGraphTask>& Tasks, const TMap<FPCGTaskId, TSet<FPCGTaskId>>& TaskSuccessors)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("POST SCHEDULE:"));

		LogGraphTasks(Tasks, &TaskSuccessors);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogPostProcessGraph(const UPCGComponent* InSourceComponent)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		UE_LOG(LogPCG, Display, TEXT("[%s/%s] UPCGComponent::PostProcessGraph"),
			(InSourceComponent && InSourceComponent->GetOwner()) ? *InSourceComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InSourceComponent && InSourceComponent->GetGraph()) ? *InSourceComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogComponentCancellation(const TSet<UPCGComponent*>& CancelledComponents)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() && !IsRunningCommandlet())
		{
			return;
		}

		for (const UPCGComponent* Component : CancelledComponents)
		{
			UE_LOG(LogPCG, Display, TEXT("[%s/%s] Component cancelled"),
				(Component && Component->GetOwner()) ? *Component->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
				(Component && Component->GetGraph()) ? *Component->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogChangeOriginIgnoredForComponent(const UObject* InObject, const UPCGComponent* InComponent)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[%s/%s] Change origin ignored: '%s'"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			InObject ? *InObject->GetName() : TEXT("MISSINGOBJECT"));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGraphExecuteFrameFinished()
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("--- FINISH FPCGGRAPHEXECUTOR::EXECUTE ---"));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	FString GetPinsToDeactivateString(const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
		FString PinIdsToDeactivateString;
		bool bFirst = true;

		for (const FPCGPinId& PinId : PinIdsToDeactivate)
		{
			const FPCGTaskId NodeId = PCGPinIdHelpers::GetNodeIdFromPinId(PinId);
			const uint64 PinIndex = PCGPinIdHelpers::GetPinIndexFromPinId(PinId);
			PinIdsToDeactivateString += bFirst ? FString::Printf(TEXT("%u_%u"), NodeId, PinIndex) : FString::Printf(TEXT(",%u_%u"), NodeId, PinIndex);
			bFirst = false;
		}

		return PinIdsToDeactivateString;
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING

	void LogTaskExecute(const FPCGGraphTask& Task)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() || !Task.SourceComponent.Get())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("         [%s/%s] %s\t\tEXECUTE"),
			*Task.SourceComponent->GetOwner()->GetName(),
			Task.SourceComponent->GetGraph() ? *Task.SourceComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			*FString::Printf(TEXT("%u'%s'"), Task.NodeId, Task.Node ? *Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("")));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogTaskExecuteCachingDisabled(const FPCGGraphTask& Task)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled() || !Task.SourceComponent.Get())
		{
			return;
		}

		UE_LOG(LogPCG, Warning, TEXT("[%s/%s] %s\t\tCACHING DISABLED"),
			*Task.SourceComponent->GetOwner()->GetName(),
			Task.SourceComponent->GetGraph() ? *Task.SourceComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			*FString::Printf(TEXT("%u'%s'"), Task.NodeId, Task.Node ? *Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("")));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogTaskCullingBegin(FPCGTaskId CompletedTaskId, uint64 InactiveOutputPinBitmask, const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("BEGIN CullInactiveDownstreamNodes, CompletedTaskId: %u, InactiveOutputPinBitmask: %u, Deactivating pin IDs: %s"),
			CompletedTaskId, InactiveOutputPinBitmask, *GetPinsToDeactivateString(PinIdsToDeactivate));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogTaskCullingBeginLoop(FPCGTaskId PinTaskId, uint64 PinIndex, const TArray<FPCGPinId>& PinIdsToDeactivate)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!CullingLogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("LOOP: DEACTIVATE %u_%u, remaining IDs: %s"), PinTaskId, PinIndex, *GetPinsToDeactivateString(PinIdsToDeactivate));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogTaskCullingUpdatedPinDeps(FPCGTaskId TaskId, const FPCGPinDependencyExpression& PinDependency, bool bDependencyExpressionBecameFalse)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!CullingLogEnabled())
		{
			return;
		}

		const FString PinDependencyString =
#if WITH_EDITOR
			PinDependency.ToString();
#else
			TEXT("MISSINGPINDEPS");
#endif

		UE_LOG(LogPCG, Log, TEXT("UPDATED PIN DEP EXPRESSION (task ID %u): %s"), TaskId, *PinDependencyString);

		if (bDependencyExpressionBecameFalse)
		{
			UE_LOG(LogPCG, Log, TEXT("CULL task ID %u"), TaskId);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteStore(const FPCGContext* InContext, EPCGHiGenGrid InGenerationGrid, int32 InFromGridSize, int32 InToGridSize, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] STORE. GenerationGridSize=%d, FromGridSize=%d, ToGridSize=%d, Path=%s"),
			*GetOwnerName(InContext->SourceComponent.Get()),
			PCGHiGenGrid::GridToGridSize(InGenerationGrid),
			InFromGridSize,
			InToGridSize,
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieve(const FPCGContext* InContext, EPCGHiGenGrid InGenerationGrid, int32 InFromGridSize, int32 InToGridSize, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE. GenerationGridSize=%d, FromGridSize=%d, ToGridSize=%d, Path=%s"),
			*GetOwnerName(InContext->SourceComponent.Get()),
			PCGHiGenGrid::GridToGridSize(InGenerationGrid),
			InFromGridSize,
			InToGridSize,
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveSuccess(const FPCGContext* InContext, const UPCGComponent* InComponent, const FString& InResourcePath, int32 InDataItemCount)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: SUCCESS. Path=%s DataItems=%d"),
			*GetOwnerName(InContext->SourceComponent.Get()),
			*InResourcePath,
			InDataItemCount);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveScheduleGraph(const FPCGContext* InContext, const UPCGComponent* InScheduledComponent, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: SCHEDULE GRAPH. Component=%s Path=%s"),
			*GetOwnerName(InContext->SourceComponent.Get()),
			*GetOwnerName(InScheduledComponent),
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveWaitOnScheduledGraph(const FPCGContext* InContext, const UPCGComponent* InWaitOnComponent, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: WAIT FOR SCHEDULED GRAPH. Component=%s Path=%s"),
			*GetOwnerName(InContext->SourceComponent.Get()),
			*GetOwnerName(InWaitOnComponent),
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}
	
	void LogGridLinkageTaskExecuteRetrieveWakeUp(const FPCGContext* InContext, const UPCGComponent* InWokenBy)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: WOKEN BY Component=%s"),
			*GetOwnerName(InContext->SourceComponent.Get()),
			*GetOwnerName(InWokenBy));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveNoLocalComponent(const FPCGContext* InContext, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Warning, TEXT("[GRIDLINKING] [%s] RETRIEVE: FAILED: No overlapping local component found. This may be expected. Path=%s"),
			*GetOwnerName(InContext->SourceComponent.Get()),
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}

	void LogGridLinkageTaskExecuteRetrieveNoData(const FPCGContext* InContext, const UPCGComponent* InComponent, const FString& InResourcePath)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		UE_LOG(LogPCG, Warning, TEXT("[GRIDLINKING] [%s] RETRIEVE: FAILED: No data found on local component. Component=%s, Path=%s"),
			*GetOwnerName(InContext->SourceComponent.Get()),
			*GetOwnerName(InComponent),
			*InResourcePath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	}
}
