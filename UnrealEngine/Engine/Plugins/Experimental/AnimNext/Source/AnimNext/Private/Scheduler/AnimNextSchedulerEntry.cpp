// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextSchedulerEntry.h"

#include "ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Engine/World.h"
#include "AnimNextStats.h"
#include "AnimNextTickFunctionBinding.h"
#include "Logging/StructuredLog.h"
#include "Param/PropertyBagProxy.h"

DEFINE_STAT(STAT_AnimNext_InitializeEntry);

FAnimNextSchedulerEntry::FAnimNextSchedulerEntry(const UAnimNextSchedule* InSchedule, UObject* InObject, UE::AnimNext::FScheduleHandle InHandle, EAnimNextScheduleInitMethod InInitMethod, TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>&& InInitializeCallback)
	: Schedule(InSchedule)
	, WeakObject(InObject)
	, Handle(InHandle)
	, Context(InSchedule, this)
	, InitializeCallback(MoveTemp(InInitializeCallback))
	, RunState(ERunState::None)
	, InitMethod(InInitMethod)
{
}

FAnimNextSchedulerEntry::~FAnimNextSchedulerEntry()
{
	Invalidate();
}

void FAnimNextSchedulerEntry::Initialize()
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_InitializeEntry);
	
	using namespace UE::AnimNext;

	check(IsInGameThread());

	check(WeakObject.IsValid())
	check(Schedule);
	check(Handle.IsValid());

	ResolvedObject = WeakObject.Get();
	UWorld* World = ResolvedObject->GetWorld();
	bIsEditor = ResolvedObject && World->WorldType == EWorldType::Editor;

	// Setup tick function graph
	if (Schedule->Instructions.Num() > 0)
	{
		// TODO: split allocation and registration into separate async tasks to reduce GT overheads of spawning
		// This will require correctly dealing with ReleaseHandle in the schedule as the two could then overlap
		TransitionToRunState(ERunState::CreatingTasks);

		RootParamStack = MakeShared<FParamStack>();

		// Allocate instance data
		Context.InstanceData = MakeUnique<FScheduleInstanceData>(Context, Schedule, Handle, this);

		FParamStack::AttachToCurrentThread(RootParamStack);

		FParamStack& ParamStack = FParamStack::Get();

		TargetObjects.SetNum(Schedule->Instructions.Num());
		
		BeginTickFunction = MakeUnique<FScheduleBeginTickFunction>(*this);
		EndTickFunction = MakeUnique<FScheduleEndTickFunction>(*this);

		TArray<TUniquePtr<FScheduleTickFunction>*> Prerequisites;
		int32 InstructionIndex = 0;
		while (InstructionIndex < Schedule->Instructions.Num())
		{
			auto AddTickFunctionAndPrerequisites = [this, &Prerequisites](TUniquePtr<FScheduleTickFunction>&& InTickFunction)
			{
				// Add a prereq on the previous tick function we added, if any (we only support linear chains right now so this is OK)
				if (TickFunctions.Num() == 0)
				{
					BeginTickFunction->Subsequent = FTickPrerequisite(ResolvedObject, *InTickFunction.Get());
					InTickFunction->AddPrerequisite(ResolvedObject, *BeginTickFunction.Get());
				}

				for (TUniquePtr<FScheduleTickFunction>* Prerequisite : Prerequisites)
				{
					Prerequisite->Get()->Subsequents.Emplace(ResolvedObject, *InTickFunction.Get());
					InTickFunction->AddPrerequisite(ResolvedObject, *Prerequisite->Get());
				}
				Prerequisites.Reset();
				TickFunctions.Add(MoveTemp(InTickFunction));
			};

			TWeakObjectPtr<UObject>& TargetObject = TargetObjects[InstructionIndex];
			const FAnimNextScheduleInstruction& Instruction = Schedule->Instructions[InstructionIndex++];
			switch (Instruction.Opcode)
			{
			case EAnimNextScheduleScheduleOpcode::BeginRunExternalTask:
				{
					AddTickFunctionAndPrerequisites(MakeUnique<FScheduleTickFunction>(
						Context,
						TConstArrayView<FAnimNextScheduleInstruction>(&Instruction, 1),
						TConstArrayView<TWeakObjectPtr<UObject>>(&TargetObject, 1)));

					const FName ExternalTaskName = Schedule->ExternalTasks[Instruction.Operand].ExternalTask;
					if(ExternalTaskName != NAME_None)
					{
						FAnimNextTickFunctionBinding* FoundBinding = ParamStack.GetMutableParamPtr<FAnimNextTickFunctionBinding>(ExternalTaskName);
						if(FoundBinding)
						{
							check(FoundBinding->Object.Get());
							check(FoundBinding->TickFunction);
							FoundBinding->TickFunction->AddPrerequisite(ResolvedObject, *TickFunctions.Last().Get());
							TargetObject = FoundBinding->Object.Get();
							TickFunctions.Last()->Subsequents.Emplace(FoundBinding->Object.Get(), *FoundBinding->TickFunction);
						}
						else
						{
							UE_LOGFMT(LogAnimation, Warning, "AnimNext: Could not bind to external tick function using binding parameters (ExternalTaskName={ExternalTaskName})", ExternalTaskName);
						}
					}
					else
					{
						UE_LOGFMT(LogAnimation, Warning, "AnimNext: Invalid external tick function binding parameters (ExternalTaskName={ExternalTaskName})", ExternalTaskName);
					}
					break;
				}
			case EAnimNextScheduleScheduleOpcode::EndRunExternalTask:
				{
					AddTickFunctionAndPrerequisites(MakeUnique<FScheduleTickFunction>(
						Context,
						TConstArrayView<FAnimNextScheduleInstruction>(&Instruction, 1),
						TConstArrayView<TWeakObjectPtr<UObject>>(&TargetObject, 1)));

					const FName ExternalTaskName = Schedule->ExternalTasks[Instruction.Operand].ExternalTask;
					if(ExternalTaskName != NAME_None)
					{
						FAnimNextTickFunctionBinding* FoundBinding = ParamStack.GetMutableParamPtr<FAnimNextTickFunctionBinding>(ExternalTaskName);
						if(FoundBinding)
						{
							TargetObject = FoundBinding->Object.Get();
							TickFunctions.Last()->AddPrerequisite(FoundBinding->Object.Get(), *FoundBinding->TickFunction);
						}
					}
					break;
				}
			case EAnimNextScheduleScheduleOpcode::RunGraphTask:
			case EAnimNextScheduleScheduleOpcode::RunPort:
			case EAnimNextScheduleScheduleOpcode::RunParamScopeEntry:
			case EAnimNextScheduleScheduleOpcode::RunParamScopeExit:
				AddTickFunctionAndPrerequisites(MakeUnique<FScheduleTickFunction>(
					Context,
					TConstArrayView<FAnimNextScheduleInstruction>(&Instruction, 1),
					TConstArrayView<TWeakObjectPtr<UObject>>(&TargetObject, 1)));
				break;
			case EAnimNextScheduleScheduleOpcode::RunExternalParamTask:
				{
					TUniquePtr<FScheduleTickFunction> NewTickFunction = MakeUnique<FScheduleTickFunction>(
						Context,
						TConstArrayView<FAnimNextScheduleInstruction>(&Instruction, 1),
						TConstArrayView<TWeakObjectPtr<UObject>>(&TargetObject, 1));
					NewTickFunction->bRunOnAnyThread = Schedule->ExternalParamTasks[Instruction.Operand].bThreadSafe;
					AddTickFunctionAndPrerequisites(MoveTemp(NewTickFunction));
					break;
				}
			case EAnimNextScheduleScheduleOpcode::PrerequisiteTask:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteBeginExternalTask:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteEndExternalTask:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteScopeEntry:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteScopeExit:
			case EAnimNextScheduleScheduleOpcode::PrerequisiteExternalParamTask:
				check(TickFunctions[Instruction.Operand].IsValid());
				Prerequisites.Add(&TickFunctions[Instruction.Operand]);
				break;
			case EAnimNextScheduleScheduleOpcode::Exit:
				check(Prerequisites.Num() == 0);
				if(TickFunctions.Num() > 0)
				{
					EndTickFunction->AddPrerequisite(ResolvedObject, *TickFunctions.Last().Get());
					TickFunctions.Last()->Subsequents.Emplace(ResolvedObject, *EndTickFunction.Get());
				}
				else
				{
					EndTickFunction->AddPrerequisite(ResolvedObject, *BeginTickFunction.Get());
					BeginTickFunction->Subsequent = FTickPrerequisite(ResolvedObject, *EndTickFunction.Get());
				}
				break;
			default:
				checkNoEntry();
				break;
			}
		}
		
		FParamStack::DetachFromCurrentThread();

		TransitionToRunState(ERunState::BindingTasks);

		// Register our tick functions
		ULevel* Level = World->PersistentLevel;
		BeginTickFunction->RegisterTickFunction(Level);
		EndTickFunction->RegisterTickFunction(Level);
		for (TUniquePtr<FScheduleTickFunction>& TickFunction : TickFunctions)
		{
			TickFunction->RegisterTickFunction(Level);
		}

		TransitionToRunState(ERunState::PendingInitialUpdate);

		if(InitializeCallback)
		{
			InitializeCallback(Context);
		}

		// Just pause now if we arent needing an initial update
		if(InitMethod == EAnimNextScheduleInitMethod::None)
		{
			Enable(false);
		}
		else
		{
			// In editor preview worlds (ideally just thumbnail scenes) we run a linearized 'initial tick' to ensure we
			// generate an output pose, as these worlds never tick
			if(World->WorldType == EWorldType::EditorPreview)
			{
				FScheduleTickFunction::RunSchedule(*this);
			}
		}
	}

	ResolvedObject = nullptr;
}

void FAnimNextSchedulerEntry::ResetBindingsAndInstanceData()
{
	using namespace UE::AnimNext;

	check(IsInGameThread());

	TransitionToRunState(ERunState::None);

	if(BeginTickFunction)
	{
		BeginTickFunction->Subsequent.PrerequisiteTickFunction->RemovePrerequisite(WeakObject.Get(), *BeginTickFunction.Get());
	}
	
	for (TUniquePtr<FScheduleTickFunction>& TickFunction : TickFunctions)
	{
		for (FTickPrerequisite& Subsequent : TickFunction->Subsequents)
		{
			Subsequent.PrerequisiteTickFunction->RemovePrerequisite(WeakObject.Get(), *TickFunction.Get());
		}
		TickFunction->UnRegisterTickFunction();
	}

	if (BeginTickFunction)
	{
		BeginTickFunction->UnRegisterTickFunction();
	}
	if(EndTickFunction)
	{
		EndTickFunction->UnRegisterTickFunction();
	}

	BeginTickFunction.Reset();
	EndTickFunction.Reset();
	TickFunctions.Reset();

	Context.InstanceData.Reset();
}

void FAnimNextSchedulerEntry::Invalidate()
{
	using namespace UE::AnimNext;

	ResetBindingsAndInstanceData();

	InitializeCallback = nullptr;
	Schedule = nullptr;
	WeakObject = nullptr;
	TargetObjects.Reset();
	Handle.Invalidate();
}

void FAnimNextSchedulerEntry::ClearTickFunctionPauseFlags()
{
	using namespace UE::AnimNext;

	check(IsInGameThread());
	
	BeginTickFunction->bTickEvenWhenPaused = false;
	for (TUniquePtr<FScheduleTickFunction>& TickFunction : TickFunctions)
	{
		TickFunction->bTickEvenWhenPaused = false;
	}
	EndTickFunction->bTickEvenWhenPaused = false;
}

void FAnimNextSchedulerEntry::Enable(bool bInEnabled)
{
	using namespace UE::AnimNext;

	check(IsInGameThread());

	BeginTickFunction->SetTickFunctionEnable(bInEnabled);
	for (TUniquePtr<FScheduleTickFunction>& TickFunction : TickFunctions)
	{
		TickFunction->SetTickFunctionEnable(bInEnabled);
	}
	EndTickFunction->SetTickFunctionEnable(bInEnabled);

	TransitionToRunState(bInEnabled ? ERunState::Running : ERunState::Paused);
}

void FAnimNextSchedulerEntry::TransitionToRunState(ERunState InNewState)
{
	switch(InNewState)
	{
	case ERunState::None:
		check(RunState == ERunState::None || RunState == ERunState::PendingInitialUpdate || RunState == ERunState::Paused || RunState == ERunState::Running);
		break;
	case ERunState::CreatingTasks:
		check(RunState == ERunState::None);
		break;
	case ERunState::BindingTasks:
		check(RunState == ERunState::CreatingTasks);
		break;
	case ERunState::PendingInitialUpdate:
		check(RunState == ERunState::BindingTasks);
		break;
	case ERunState::Running:
		check(RunState == ERunState::PendingInitialUpdate || RunState == ERunState::Paused || RunState == ERunState::Running);
		break;
	case ERunState::Paused:
		check(RunState == ERunState::PendingInitialUpdate || RunState == ERunState::Paused || RunState == ERunState::Running);
		break;
	default:
		checkNoEntry();
	}

	RunState = InNewState;
}

#if WITH_EDITOR
void FAnimNextSchedulerEntry::OnScheduleCompiled()
{
	using namespace UE::AnimNext;

	// Store any user-defined scopes, as the instance data will be going away
	TUniquePtr<FPropertyBagProxy> RootUserScope = MoveTemp(Context.InstanceData->RootUserScope);
	TMap<FName, FScheduleInstanceData::FUserScope> UserScopes = MoveTemp(Context.InstanceData->UserScopes);

	ResetBindingsAndInstanceData();
	Initialize();

	// Restore any user scopes to the recreated instance data
	Context.InstanceData->RootUserScope = MoveTemp(RootUserScope);
	Context.InstanceData->UserScopes = MoveTemp(UserScopes);
}
#endif