// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleTickFunction.h"

#include "Param/PropertyBagProxy.h"
#include "Scheduler/ScheduleContext.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/AnimNextScheduleGraphTask.h"
#include "Scheduler/AnimNextSchedulePortTask.h"
#include "Scheduler/AnimNextScheduleExternalTask.h"
#include "Scheduler/AnimNextScheduleParamScopeTask.h"
#include "Scheduler/ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedulerWorldSubsystem.h"

namespace UE::AnimNext
{

void FScheduleBeginTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Run(DeltaTime);
}

void FScheduleBeginTickFunction::Run(float DeltaTime)
{
	while (!PreExecuteTasks.IsEmpty())
	{
		TOptional<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> Function = PreExecuteTasks.Dequeue();
		check(Function.IsSet());
		Function.GetValue()(Entry.Context);
	}

	// Push any user layer we have at the root
	FScheduleInstanceData& InstanceData = Entry.Context.GetInstanceData();
	if(InstanceData.RootUserScope.IsValid())
	{
		InstanceData.PushedRootUserLayer = InstanceData.RootParamStack->PushLayer(InstanceData.RootUserScope->GetLayerHandle());
	}

	Entry.ResolvedObject = Entry.WeakObject.Get();
	Entry.DeltaTime = DeltaTime;
}

FString FScheduleBeginTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleBeginTickFunction");
}

void FScheduleEndTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Run();
}

void FScheduleEndTickFunction::Run()
{
	// Pop any user layer we have at the root
	FScheduleInstanceData& InstanceData = Entry.Context.GetInstanceData();
	InstanceData.RootParamStack->PopLayer(InstanceData.PushedRootUserLayer);

	auto RunTaskOnGameThread = [](TUniqueFunction<void(void)>&& InFunction)
	{
		if(IsInGameThread())
		{
			InFunction();
		}
		else
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(InFunction), TStatId(), nullptr, ENamedThreads::GameThread);
		}
	};
	
	if(Entry.RunState == FAnimNextSchedulerEntry::ERunState::PendingInitialUpdate)
	{
		if( Entry.InitMethod == EAnimNextScheduleInitMethod::InitializeAndPause
#if WITH_EDITOR
			|| (Entry.InitMethod == EAnimNextScheduleInitMethod::InitializeAndPauseInEditor && Entry.bIsEditor)
#endif
			)
		{
			// Queue task to disable our tick functions now we have performed our initial update
			RunTaskOnGameThread([this]()
			{
				check(IsInGameThread());
				Entry.Enable(false);
			});
		}
	}
	else
	{
		RunTaskOnGameThread([this]()
		{
			check(IsInGameThread());
			Entry.TransitionToRunState(FAnimNextSchedulerEntry::ERunState::Running);
		});
	}

	Entry.ResolvedObject = nullptr;
}

FString FScheduleEndTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleEndTickFunction");
}

void FScheduleTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Run();
}

void FScheduleTickFunction::Run()
{
	RunScheduleHelper(ScheduleContext, Instructions, TargetObjects,
		[this]()
		{
			while (!PreExecuteTasks.IsEmpty())
			{
				TOptional<TUniqueFunction<void(const FScheduleContext&)>> Function = PreExecuteTasks.Dequeue();
				check(Function.IsSet());
				Function.GetValue()(ScheduleContext);
			}
		}, 
		[this]()
		{
			while (!PostExecuteTasks.IsEmpty())
			{
				TOptional<TUniqueFunction<void(const FScheduleContext&)>> Function = PostExecuteTasks.Dequeue();
				check(Function.IsSet());
				Function.GetValue()(ScheduleContext);
			}
		});
}

void FScheduleTickFunction::RunSchedule(const FAnimNextSchedulerEntry& InEntry)
{
	InEntry.BeginTickFunction->Run(0.0f);
	for(const TUniquePtr<FScheduleTickFunction>& TickFunction : InEntry.TickFunctions)
	{
		TickFunction->Run();
	}
	InEntry.EndTickFunction->Run();
}

void FScheduleTickFunction::RunScheduleHelper(const FScheduleContext& InScheduleContext, TConstArrayView<FAnimNextScheduleInstruction> InInstructions, TConstArrayView<TWeakObjectPtr<UObject>> InTargetObjects, TFunctionRef<void(void)> InPreExecuteScope, TFunctionRef<void(void)> InPostExecuteScope)
{
	const UAnimNextSchedule* Schedule = InScheduleContext.Schedule;

	int32 InstructionIndex = 0;
	while(InstructionIndex < InInstructions.Num())
	{
		const FAnimNextScheduleInstruction& Instruction = InInstructions[InstructionIndex];
		switch (Instruction.Opcode)
		{
		case EAnimNextScheduleScheduleOpcode::RunGraphTask:
			{
				uint32 GraphTaskIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.GetParamStack(Schedule->GraphTasks[GraphTaskIndex].ParamScopeIndex), FParamStack::ECoalesce::Coalesce);

				Schedule->GraphTasks[GraphTaskIndex].RunGraph(InScheduleContext);

				FParamStack::DetachFromCurrentThread(FParamStack::EDecoalesce::Decoalesce);
				break;
			}
		case EAnimNextScheduleScheduleOpcode::BeginRunExternalTask:
			{
				if(InTargetObjects.Num() > 0)
				{
					if (const UObject* Object = InTargetObjects[InstructionIndex].Get())
					{
						FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
						uint32 ExternalTaskIndex = Instruction.Operand;
						FParamStack::AddForPendingObject(Object, InstanceData.GetParamStack(Schedule->ExternalTasks[ExternalTaskIndex].ParamScopeIndex));
					}
				}
				break;
			}
		case EAnimNextScheduleScheduleOpcode::EndRunExternalTask:
			{
				if(InTargetObjects.Num() > 0)
				{
					if (const UObject* Object = InTargetObjects[InstructionIndex].Get())
					{
						FParamStack::RemoveForPendingObject(Object);
					}
				}
				break;
			}
		case EAnimNextScheduleScheduleOpcode::RunPort:
			{
				uint32 PortIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.GetParamStack(Schedule->Ports[PortIndex].ParamScopeIndex));

				Schedule->Ports[PortIndex].RunPort(InScheduleContext);

				FParamStack::DetachFromCurrentThread();
				break;
			}
		case EAnimNextScheduleScheduleOpcode::RunParamScopeEntry:
			{
				InPreExecuteScope();

				uint32 ScopeEntryIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.GetParamStack(Schedule->ParamScopeEntryTasks[ScopeEntryIndex].ParamScopeIndex), FParamStack::ECoalesce::Coalesce);

				Schedule->ParamScopeEntryTasks[ScopeEntryIndex].RunParamScopeEntry(InScheduleContext);

				FParamStack::DetachFromCurrentThread();

				InPostExecuteScope();

				break;
			}
		case EAnimNextScheduleScheduleOpcode::RunParamScopeExit:
			{
				uint32 ScopeExitIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.GetParamStack(Schedule->ParamScopeExitTasks[ScopeExitIndex].ParamScopeIndex));

				Schedule->ParamScopeExitTasks[ScopeExitIndex].RunParamScopeExit(InScheduleContext);

				FParamStack::DetachFromCurrentThread(FParamStack::EDecoalesce::Decoalesce);
				break;
			}
		case EAnimNextScheduleScheduleOpcode::RunExternalParamTask:
			{
				uint32 ExternalParamIndex = Instruction.Operand;
				FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
				FParamStack::AttachToCurrentThread(InstanceData.RootParamStack);

				Schedule->ExternalParamTasks[ExternalParamIndex].UpdateExternalParams(InScheduleContext);

				FParamStack::DetachFromCurrentThread();
				break;
			}
		default:
			break;
		}

		InstructionIndex++;
	}
}

FString FScheduleTickFunction::DiagnosticMessage()
{
	return TEXT("AnimNextScheduleTickFunction");
}

}