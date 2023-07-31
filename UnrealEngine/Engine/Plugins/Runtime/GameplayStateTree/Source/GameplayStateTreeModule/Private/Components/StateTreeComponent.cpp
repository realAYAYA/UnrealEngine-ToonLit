// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/StateTreeComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayTasksComponent.h"
#include "VisualLogger/VisualLogger.h"
#include "StateTree.h"
#include "StateTreeEvaluatorBase.h"
#include "Conditions/StateTreeCommonConditions.h"
#include "AIController.h"
#include "Components/StateTreeComponentSchema.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"

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

bool UStateTreeComponent::SetContextRequirements(FStateTreeExecutionContext& Context, bool bLogErrors)
{
	if (!Context.IsValid())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	for (const FStateTreeExternalDataDesc& ItemDesc : Context.GetExternalDataDescs())
	{
		if (ItemDesc.Struct != nullptr)
		{
			if (ItemDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
			{
				UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(ToRawPtr(ItemDesc.Struct))));
				Context.SetExternalData(ItemDesc.Handle, FStateTreeDataView(Subsystem));
			}
			else if (ItemDesc.Struct->IsChildOf(UActorComponent::StaticClass()))
			{
				UActorComponent* Component = GetOwner()->FindComponentByClass(Cast<UClass>(const_cast<UStruct*>(ToRawPtr(ItemDesc.Struct))));
				Context.SetExternalData(ItemDesc.Handle, FStateTreeDataView(Component));
			}
			else if (ItemDesc.Struct->IsChildOf(APawn::StaticClass()))
			{
				APawn* OwnerPawn = (AIOwner != nullptr) ? AIOwner->GetPawn() : Cast<APawn>(GetOwner());
				Context.SetExternalData(ItemDesc.Handle, FStateTreeDataView(OwnerPawn));
			}
			else if (ItemDesc.Struct->IsChildOf(AAIController::StaticClass()))
			{
				AAIController* OwnerController = (AIOwner != nullptr) ? AIOwner.Get() : Cast<AAIController>(GetOwner());
				Context.SetExternalData(ItemDesc.Handle, FStateTreeDataView(OwnerController));
			}
			else if (ItemDesc.Struct->IsChildOf(AActor::StaticClass()))
			{
				AActor* OwnerActor = (AIOwner != nullptr) ? AIOwner->GetPawn() : GetOwner();
				Context.SetExternalData(ItemDesc.Handle, FStateTreeDataView(OwnerActor));
			}
		}
	}

	// Make sure the actor matches one required.
	AActor* ContextActor = nullptr;
	const UStateTreeComponentSchema* Schema = Cast<UStateTreeComponentSchema>(Context.GetStateTree()->GetSchema());
	if (Schema)
	{
		if (AAIController* OwnerController = (AIOwner != nullptr) ? AIOwner.Get() : Cast<AAIController>(GetOwner()))
		{
			if (OwnerController && OwnerController->IsA(Schema->GetContextActorClass()))
			{
				ContextActor = OwnerController;
			}
		}
		if (ContextActor == nullptr)
		{
			if (AActor* OwnerActor = (AIOwner != nullptr) ? AIOwner->GetPawn() : GetOwner())
			{
				if (OwnerActor && OwnerActor->IsA(Schema->GetContextActorClass()))
				{
					ContextActor = OwnerActor;
				}
			}
		}
		if (ContextActor == nullptr && bLogErrors)
		{
			STATETREE_LOG(Error, TEXT("%s: Could not find context actor of type %s. StateTree will not update."), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Schema->GetContextActorClass()));
		}
	}
	else if (bLogErrors)
	{
		STATETREE_LOG(Error, TEXT("%s: Expected StateTree asset to contain StateTreeComponentSchema. StateTree will not update."), ANSI_TO_TCHAR(__FUNCTION__));
	}
	
	const FName ActorName(TEXT("Actor"));
	for (const FStateTreeExternalDataDesc& ItemDesc : Context.GetContextDataDescs())
	{
		if (ItemDesc.Name == ActorName)
		{
			Context.SetExternalData(ItemDesc.Handle, FStateTreeDataView(ContextActor));
		}
	}

	bool bResult = Context.AreExternalDataViewsValid();

	if (!bResult && bLogErrors)
	{
		STATETREE_LOG(Error, TEXT("%s: Missing external data requirements. StateTree will not update."), ANSI_TO_TCHAR(__FUNCTION__));
	}
	
	return bResult;
}

void UStateTreeComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (AIOwner == nullptr && bStartLogicAutomatically)
	{
		StartLogic();
	}
}

void UStateTreeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsRunning || bIsPaused)
	{
		return;
	}

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (SetContextRequirements(Context))
	{
		Context.Tick(DeltaTime);
	}
}

void UStateTreeComponent::StartLogic()
{
	STATETREE_LOG(Log, TEXT("%s: Start Logic"), ANSI_TO_TCHAR(__FUNCTION__));

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (SetContextRequirements(Context))
	{
		Context.SetParameters(StateTreeRef.GetParameters());
		Context.Start();
		bIsRunning = true;
	}
}

void UStateTreeComponent::RestartLogic()
{
	STATETREE_LOG(Log, TEXT("%s: Restart Logic"), ANSI_TO_TCHAR(__FUNCTION__));

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (SetContextRequirements(Context))
	{
		Context.SetParameters(StateTreeRef.GetParameters());
		Context.Start();
		bIsRunning = true;
	}
}

void UStateTreeComponent::StopLogic(const FString& Reason)
{
	STATETREE_LOG(Log, TEXT("%s: Stopping, reason: \'%s\'"), ANSI_TO_TCHAR(__FUNCTION__), *Reason);

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (SetContextRequirements(Context))
	{
		Context.Stop();
		bIsRunning = false;
	}
}

void UStateTreeComponent::Cleanup()
{
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

void UStateTreeComponent::SendStateTreeEvent(const FStateTreeEvent& Event)
{
	if (!bIsRunning)
	{
		STATETREE_LOG(Warning, TEXT("%s: Trying to send even to a StateTree that is not started yet."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	if (Context.IsValid())
	{
		Context.SendEvent(Event);
	}
}


#if WITH_GAMEPLAY_DEBUGGER
FString UStateTreeComponent::GetDebugInfoString() const
{
	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), const_cast<FStateTreeInstanceData&>(InstanceData));
	return Context.GetDebugInfoString();
}
#endif // WITH_GAMEPLAY_DEBUGGER

#undef STATETREE_LOG
#undef STATETREE_CLOG
