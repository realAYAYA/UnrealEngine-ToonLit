// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/StateTreeComponent.h"
#include "GameplayTasksComponent.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"
#include "AIController.h"
#include "Components/StateTreeComponentSchema.h"
#include "Engine/World.h"
#include "Tasks/AITask.h"

#define STATETREE_LOG(Verbosity, Format, ...) UE_VLOG(GetOwner(), LogStateTree, Verbosity, Format, ##__VA_ARGS__)
#define STATETREE_CLOG(Condition, Verbosity, Format, ...) UE_CVLOG((Condition), GetOwner(), LogStateTree, Verbosity, Format, ##__VA_ARGS__)

//////////////////////////////////////////////////////////////////////////
// UStateTreeComponent

UStateTreeComponent::UStateTreeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	bIsRunning = false;
	bIsPaused = false;
}

void UStateTreeComponent::InitializeComponent()
{
	if (!StateTreeRef.IsValid())
	{
		STATETREE_LOG(Error, TEXT("%s: StateTree asset is not set, cannot initialize."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	const FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (!Context.IsValid())
	{
		STATETREE_LOG(Error, TEXT("%s: Failed to init StateTreeContext."), ANSI_TO_TCHAR(__FUNCTION__));
	}
}

#if WITH_EDITOR
void UStateTreeComponent::PostLoad()
{
	Super::PostLoad();
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (StateTree_DEPRECATED != nullptr)
	{
		StateTreeRef.SetStateTree(StateTree_DEPRECATED);
		StateTreeRef.SyncParameters();
		StateTree_DEPRECATED = nullptr;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif //WITH_EDITOR

void UStateTreeComponent::UninitializeComponent()
{
}

bool UStateTreeComponent::CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, TArrayView<FStateTreeDataView> OutDataViews) const
{
	return UStateTreeComponentSchema::CollectExternalData(Context, StateTree, ExternalDataDescs, OutDataViews);
}

bool UStateTreeComponent::SetContextRequirements(FStateTreeExecutionContext& Context, bool bLogErrors)
{
	Context.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateUObject(this, &UStateTreeComponent::CollectExternalData));
	return UStateTreeComponentSchema::SetContextRequirements(*this, Context);
}

void UStateTreeComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (AIOwner == nullptr && bStartLogicAutomatically)
	{
		StartLogic();
	}
}

void UStateTreeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLogic(UEnum::GetValueAsString(EndPlayReason));

	Super::EndPlay(EndPlayReason);
}

void UStateTreeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsRunning || bIsPaused)
	{
		return;
	}
	
	if (!StateTreeRef.IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: Trying to tick State Tree component with invalid asset."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (SetContextRequirements(Context))
	{
		const EStateTreeRunStatus PreviousRunStatus = Context.GetStateTreeRunStatus();
		const EStateTreeRunStatus CurrentRunStatus = Context.Tick(DeltaTime);

		if (CurrentRunStatus != PreviousRunStatus)
		{
			OnStateTreeRunStatusChanged.Broadcast(CurrentRunStatus);
		}
	}
}

void UStateTreeComponent::StartLogic()
{
	STATETREE_LOG(Log, TEXT("%s: Start Logic"), ANSI_TO_TCHAR(__FUNCTION__));

	if (!StateTreeRef.IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: Trying to start State Tree component with invalid asset."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (SetContextRequirements(Context))
	{
		const EStateTreeRunStatus PreviousRunStatus = Context.GetStateTreeRunStatus();
		const EStateTreeRunStatus CurrentRunStatus = Context.Start(&StateTreeRef.GetParameters());
		bIsRunning = true;
		
		if (CurrentRunStatus != PreviousRunStatus)
		{
			OnStateTreeRunStatusChanged.Broadcast(CurrentRunStatus);
		}
	}
}

void UStateTreeComponent::RestartLogic()
{
	STATETREE_LOG(Log, TEXT("%s: Restart Logic"), ANSI_TO_TCHAR(__FUNCTION__));

	if (!StateTreeRef.IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: Trying to restart State Tree component with invalid asset."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (SetContextRequirements(Context))
	{
		const EStateTreeRunStatus PreviousRunStatus = Context.GetStateTreeRunStatus();
		const EStateTreeRunStatus CurrentRunStatus = Context.Start(&StateTreeRef.GetParameters());
		bIsRunning = true;
		
		if (CurrentRunStatus != PreviousRunStatus)
		{
			OnStateTreeRunStatusChanged.Broadcast(CurrentRunStatus);
		}
	}
}

void UStateTreeComponent::StopLogic(const FString& Reason)
{
	STATETREE_LOG(Log, TEXT("%s: Stopping, reason: \'%s\'"), ANSI_TO_TCHAR(__FUNCTION__), *Reason);

	if (!bIsRunning)
	{
		return;
	}

	if (!StateTreeRef.IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: Trying to stop State Tree component with invalid asset."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (SetContextRequirements(Context))
	{
		const EStateTreeRunStatus PreviousRunStatus = Context.GetStateTreeRunStatus();
		const EStateTreeRunStatus CurrentRunStatus = Context.Stop();
		bIsRunning = false;

		if (CurrentRunStatus != PreviousRunStatus)
		{
			OnStateTreeRunStatusChanged.Broadcast(CurrentRunStatus);
		}
	}
}

void UStateTreeComponent::Cleanup()
{
	StopLogic(TEXT("Cleanup"));
}

void UStateTreeComponent::PauseLogic(const FString& Reason)
{
	STATETREE_LOG(Log, TEXT("%s: Execution updates: PAUSED (%s)"), ANSI_TO_TCHAR(__FUNCTION__), *Reason);
	bIsPaused = true;
}

EAILogicResuming::Type UStateTreeComponent::ResumeLogic(const FString& Reason)
{
	STATETREE_LOG(Log, TEXT("%s: Execution updates: RESUMED (%s)"), ANSI_TO_TCHAR(__FUNCTION__), *Reason);

	const EAILogicResuming::Type SuperResumeResult = Super::ResumeLogic(Reason);

	if (!!bIsPaused)
	{
		bIsPaused = false;

		if (SuperResumeResult == EAILogicResuming::Continue)
		{
			// Nop
		}
		else if (SuperResumeResult == EAILogicResuming::RestartedInstead)
		{
			RestartLogic();
		}
	}

	return SuperResumeResult;
}

bool UStateTreeComponent::IsRunning() const
{
	return bIsRunning;
}

bool UStateTreeComponent::IsPaused() const
{
	return bIsPaused;
}

UGameplayTasksComponent* UStateTreeComponent::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	const UAITask* AITask = Cast<const UAITask>(&Task);
	return (AITask && AITask->GetAIController()) ? AITask->GetAIController()->GetGameplayTasksComponent(Task) : Task.GetGameplayTasksComponent();
}

AActor* UStateTreeComponent::GetGameplayTaskOwner(const UGameplayTask* Task) const
{
	if (Task == nullptr)
	{
		return GetAIOwner();
	}

	const UAITask* AITask = Cast<const UAITask>(Task);
	if (AITask)
	{
		return AITask->GetAIController();
	}

	const UGameplayTasksComponent* TasksComponent = Task->GetGameplayTasksComponent();
	return TasksComponent ? TasksComponent->GetGameplayTaskOwner(Task) : nullptr;
}

AActor* UStateTreeComponent::GetGameplayTaskAvatar(const UGameplayTask* Task) const
{
	if (Task == nullptr)
	{
		return GetAIOwner() ? GetAIOwner()->GetPawn() : nullptr;
	}

	const UAITask* AITask = Cast<const UAITask>(Task);
	if (AITask)
	{
		return AITask->GetAIController() ? AITask->GetAIController()->GetPawn() : nullptr;
	}

	const UGameplayTasksComponent* TasksComponent = Task->GetGameplayTasksComponent();
	return TasksComponent ? TasksComponent->GetGameplayTaskAvatar(Task) : nullptr;
}

uint8 UStateTreeComponent::GetGameplayTaskDefaultPriority() const
{
	return static_cast<uint8>(EAITaskPriority::AutonomousAI);
}

void UStateTreeComponent::OnGameplayTaskInitialized(UGameplayTask& Task)
{
	const UAITask* AITask = Cast<const UAITask>(&Task);
	if (AITask && (AITask->GetAIController() == nullptr))
	{
		// this means that the task has either been created without specifying 
		// UAITAsk::OwnerController's value (like via BP's Construct Object node)
		// or it has been created in C++ with inappropriate function
		UE_LOG(LogStateTree, Error, TEXT("Missing AIController in AITask %s"), *AITask->GetName());
	}
}

TSubclassOf<UStateTreeSchema> UStateTreeComponent::GetSchema() const
{
	return UStateTreeComponentSchema::StaticClass();
}

void UStateTreeComponent::SetStartLogicAutomatically(const bool bInStartLogicAutomatically)
{
	bStartLogicAutomatically = bInStartLogicAutomatically;
}

void UStateTreeComponent::SendStateTreeEvent(const FStateTreeEvent& Event)
{
	SendStateTreeEvent(Event.Tag, Event.Payload, Event.Origin);
}

void UStateTreeComponent::SendStateTreeEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin)
{
	if (!bIsRunning)
	{
		STATETREE_LOG(Warning, TEXT("%s: Trying to send event to a State Tree that is not started yet."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if (!StateTreeRef.IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: Trying to send event to State Tree component with invalid asset."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	InstanceData.GetMutableEventQueue().SendEvent(this, Tag, Payload, Origin);
}

EStateTreeRunStatus UStateTreeComponent::GetStateTreeRunStatus() const
{
	if (const FStateTreeExecutionState* Exec = InstanceData.GetExecutionState())
	{
		return Exec->TreeRunStatus;
	}

	return EStateTreeRunStatus::Failed;
}

#if WITH_GAMEPLAY_DEBUGGER
FString UStateTreeComponent::GetDebugInfoString() const
{
	if (!StateTreeRef.IsValid())
	{
		return FString("No StateTree to run.");
	}

	return FStateTreeExecutionContext(*GetOwner(), *StateTreeRef.GetStateTree(), const_cast<FStateTreeInstanceData&>(InstanceData)).GetDebugInfoString();
}
#endif // WITH_GAMEPLAY_DEBUGGER

#undef STATETREE_LOG
#undef STATETREE_CLOG
