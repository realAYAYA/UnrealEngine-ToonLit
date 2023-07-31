// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameplayTask_PlayContextualAnim.h"

#include "ContextualAnimManager.h"
#include "ContextualAnimSceneAsset.h"
#include "ContextualAnimSceneInstance.h"
#include "Net/UnrealNetwork.h"
#include "VisualLogger/VisualLogger.h"
#include "AIResources.h"
#include "GameplayActuationComponent.h"
#include "GameplayTasksComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTask_PlayContextualAnim)


//-----------------------------------------------------
// FGameplayActuationState_ContextualAnim
//-----------------------------------------------------
void FGameplayActuationState_ContextualAnim::OnStateDeactivated(const FConstStructView NextState)
{
	const FGameplayActuationState_ContextualAnim* State = NextState.GetPtr<FGameplayActuationState_ContextualAnim>();

	if (State == nullptr || State->SceneInstance != SceneInstance)
	{
		// If next state is not a contextual animation, stop the scene.
		if (SceneInstance != nullptr)
		{
			SceneInstance->Stop();
			SceneInstance = nullptr;
		}
	}
}


//-----------------------------------------------------
// FGameplayTransitionDesc_EnterContextualAnim
//-----------------------------------------------------
UGameplayTask* FGameplayTransitionDesc_EnterContextualAnim::MakeTransitionTask(const FMakeGameplayTransitionTaskContext& Context) const
{
	const FGameplayActuationState_ContextualAnim* NextState = Context.NextActuationState.GetPtr<FGameplayActuationState_ContextualAnim>();

	if (NextState == nullptr)
	{
		return nullptr;
	}

	UGameplayTask_PlayContextualAnim* NewTransitionTask = UGameplayTask_PlayContextualAnim::CreateContextualAnimTransition(
		Context.Actor,
		NextState->InteractorRole,
		NextState->InteractableObject,
		NextState->InteractableObjectRole,
		NextState->SectionName,
		NextState->ExitSectionName,
		NextState->SceneAsset);

	return NewTransitionTask;
}


//-----------------------------------------------------
// FGameplayTransitionDesc_ExitContextualAnim
//-----------------------------------------------------
UGameplayTask* FGameplayTransitionDesc_ExitContextualAnim::MakeTransitionTask(const FMakeGameplayTransitionTaskContext& Context) const
{
	const FGameplayActuationState_ContextualAnim* CurrentState = Context.CurrentActuationState.GetPtr<FGameplayActuationState_ContextualAnim>();
	const FGameplayActuationState_ContextualAnim* NextStateAsContextualAnim = Context.NextActuationState.GetPtr<FGameplayActuationState_ContextualAnim>();

	const bool bIsNextContextualAnim = NextStateAsContextualAnim != nullptr;

	if (CurrentState == nullptr || bIsNextContextualAnim)
	{
		return nullptr;
	}

	if (Context.Actor == nullptr
		|| CurrentState->SceneAsset == nullptr
		|| CurrentState->InteractableObject == nullptr
		|| CurrentState->ExitSectionName.IsNone())
	{
		return nullptr;
	}

	UGameplayTask_PlayContextualAnim* NewTransitionTask = UGameplayTask_PlayContextualAnim::CreateContextualAnimTransition(
		Context.Actor,
		CurrentState->InteractorRole,
		CurrentState->InteractableObject,
		CurrentState->InteractableObjectRole,
		CurrentState->ExitSectionName,
		/* ExitSectionName */ NAME_None,
		CurrentState->SceneAsset);

	return NewTransitionTask;
}


//-----------------------------------------------------
// UGameplayTask_PlayContextualAnim
//-----------------------------------------------------
UGameplayTask_PlayContextualAnim::UGameplayTask_PlayContextualAnim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSimulatedTask = true;
	bTickingTask = true;
}

void UGameplayTask_PlayContextualAnim::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	Super::InitSimulatedTask(InGameplayTasksComponent);

	SharedInitAndApply();
	UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s: %s - %s"), ANSI_TO_TCHAR(__FUNCTION__), *GetName(), *UEnum::GetValueAsString(Status));
}

UGameplayTask_PlayContextualAnim* UGameplayTask_PlayContextualAnim::PlayContextualAnim(
	AActor* Interactor
	, const FName InteractorRole
	, AActor* InteractableObject
	, const FName InteractableObjectRole
	, const FName SectionName
	, const FName ExitSectionName
	, const UContextualAnimSceneAsset* SceneAsset
	)
{
	if (Interactor == nullptr)
	{
		return nullptr;
	}

	const UGameplayActuationComponent* InteractorActuationComponent = Interactor->FindComponentByClass<UGameplayActuationComponent>(); 
	if (InteractorActuationComponent == nullptr)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: missing UGameplayActuationComponent."));
		return nullptr;
	}

	if (InteractableObject == nullptr)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: missing InteractableObject."));
		return nullptr;
	}

	if (SceneAsset == nullptr)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: missing SceneAsset."));
		return nullptr;
	}
	
	const int32 SectionIdx = SceneAsset->GetSectionIndex(SectionName);
	if (SectionIdx == INDEX_NONE)
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: no Section '%s' in '%s'"), *SectionName.ToString(), *SceneAsset->GetName());
		return nullptr;
	}

	// Precompute the pivots so they are ready to start the scene on all clients.
	FContextualAnimStartSceneParams SceneParams;
	SceneParams.RoleToActorMap.Add(InteractorRole, Interactor);
	SceneParams.RoleToActorMap.Add(InteractableObjectRole, InteractableObject);
	SceneParams.SectionIdx = SectionIdx;

	TArray<FContextualAnimSetPivot> Pivots;
	FContextualAnimSceneBindings Bindings;
	if (CreateBindings(*SceneAsset, SceneParams, Bindings))
	{
		Bindings.CalculateAnimSetPivots(Pivots);
	}
	else
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("Can't create task: unable to find matching bindings or force some."));
		return nullptr;
	}
	
	// Create task and set all data required for initialization (both for main task and the replicated one)
	UGameplayTask_PlayContextualAnim* Task = NewTask<UGameplayTask_PlayContextualAnim>(Interactor);
	Task->InteractorRole = InteractorRole;
	Task->InteractableObject = InteractableObject;
	Task->InteractableObjectRole = InteractableObjectRole;
	Task->ExitSectionName = ExitSectionName;
	Task->SceneAsset = SceneAsset;
	Task->SectionIdx = SectionIdx;
	Task->AnimSetIdx = Bindings.GetAnimSetIdx();
	Task->Pivots.Reserve(Pivots.Num());
	for (const FContextualAnimSetPivot& Pivot : Pivots)
	{
		Task->Pivots.Add(Pivot.Transform);
	}

	Task->AddClaimedResource<UAIResource_Movement>();
	Task->AddRequiredResource<UAIResource_Movement>();
	Task->ActuationState.ActuationName = FName("PlayCA");
	Task->ResourceOverlapPolicy = ETaskResourceOverlapPolicy::RequestCancelAndStartAtEnd;

	return Task;
}

UGameplayTask_PlayContextualAnim* UGameplayTask_PlayContextualAnim::CreateContextualAnimTransition(
	AActor* Interactor,
	const FName InteractorRole,
	AActor* InteractableObject,
	const FName InteractableObjectRole,
	const FName SectionName,
	const FName ExitSectionName,
	const UContextualAnimSceneAsset* SceneAsset
	)
{
	UGameplayTask_PlayContextualAnim* Task = PlayContextualAnim(Interactor, InteractorRole, InteractableObject, InteractableObjectRole, SectionName, ExitSectionName, SceneAsset);
	Task->ActuationState.ActuationName = FName("PlayCATransition");
	Task->ResourceOverlapPolicy = ETaskResourceOverlapPolicy::StartOnTop;
	Task->Priority = 128;

	return Task;
}

void UGameplayTask_PlayContextualAnim::Activate()
{
	Super::Activate();
	SharedInitAndApply();
	UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s: %s - %s"), ANSI_TO_TCHAR(__FUNCTION__), *GetName(), *UEnum::GetValueAsString(Status));

	if (Status == EPlayContextualAnimStatus::Failed)
	{
		OnRequestFailed.Broadcast();
	}
}

void UGameplayTask_PlayContextualAnim::TransitionToSection()
{
	Status = EPlayContextualAnimStatus::Failed;
	ActuationState.SceneInstance = nullptr;

	if (!ensureMsgf(SceneParams.SectionIdx != INDEX_NONE && SceneParams.AnimSetIdx != INDEX_NONE,
		TEXT("SceneParam Section index and AnimSet index must be set before calling '%s'"), ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return;
	}
	
	if (!ensureMsgf(SceneAsset != nullptr,
		TEXT("SceneAsset must be set before calling '%s'"), ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return;
	}

	bool bSuccess = false;

	if (SceneInstance == nullptr)
	{
		const FContextualAnimSceneSection* Section = SceneAsset->GetSection(SceneParams.SectionIdx);
		UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Starting scene at Section '%s'"),
			Section != nullptr ? *Section->GetName().ToString() : *LexToString(SceneParams.SectionIdx));

		UContextualAnimManager* ContextualAnimManager = UContextualAnimManager::Get(GetWorld());
		SceneInstance = ContextualAnimManager != nullptr ? ContextualAnimManager->ForceStartScene(*SceneAsset, SceneParams) : nullptr;
		bSuccess = SceneInstance != nullptr;
		UE_CVLOG_UELOG(!bSuccess, GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("Unable to start scene for specified section"));
	}
	else
	{
		const FContextualAnimSceneSection* PrevSection = SceneAsset->GetSection(SceneInstance->GetBindings().GetSectionIdx());
		const FContextualAnimSceneSection* NextSection = SceneAsset->GetSection(SceneParams.SectionIdx);
		UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Transitioning from Section '%s' to '%s'"),
			PrevSection != nullptr ? *PrevSection->GetName().ToString() : *LexToString(SceneInstance->GetBindings().GetSectionIdx()),
			NextSection != nullptr ? *NextSection->GetName().ToString() : *LexToString(SceneParams.SectionIdx));

		bSuccess = SceneInstance->ForceTransitionToSection(SceneParams.SectionIdx, SceneParams.AnimSetIdx, SceneParams.Pivots);
		UE_CVLOG_UELOG(!bSuccess, GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("Unable to transition to specified section"));
	}

	if (bSuccess)
	{
		check(SceneInstance != nullptr);
		SceneInstance->OnSectionEndTimeReached.AddDynamic(this, &UGameplayTask_PlayContextualAnim::OnSectionEndTimeReached);

		// Compute safe exit point now since all contexts are valid (might now be the case on exit if some actors are gone)
		// Use current actor transform as fallback but overwrite using default exit section last frame alignment point  
		const AActor* Interactor = GetAvatarActor();
		check(Interactor != nullptr);
		SafeExitPoint = Interactor->GetActorTransform();

		if (const FContextualAnimSceneBinding* PrimaryRoleBinding = SceneInstance->GetBindings().FindBindingByRole(SceneAsset->GetPrimaryRole()))
		{
			TArray<FContextualAnimPoint> Points;
			const int32 ExitSectionIdx = SceneAsset->GetSectionIndex(ExitSectionName);
			SceneAsset->GetAlignmentPointsForSecondaryRole(EContextualAnimPointType::LastFrame, ExitSectionIdx, PrimaryRoleBinding->GetContext(), Points);
			if (Points.Num())
			{
				SafeExitPoint = Points[0].Transform;
			}
		}

		ActuationState.SceneInstance = SceneInstance;
		Status = EPlayContextualAnimStatus::Playing;
	}
}

void UGameplayTask_PlayContextualAnim::SetExit(const EPlayContextualAnimExitMode ExitMode, const FName NewExitSectionName)
{
	if (ExitMode == EPlayContextualAnimExitMode::Teleport)
	{
		// Clear exit section name so no transition task will be created
		ActuationState.ExitSectionName = NAME_None;

		// Keep track on the teleport request when task will end
		bTeleportOnTaskEnd = true;
	}
	else
	{
		// Update actuation state so transition can use appropriate exit section
		ActuationState.ExitSectionName = NewExitSectionName;
	}
}

void UGameplayTask_PlayContextualAnim::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	DOREPLIFETIME_CONDITION(UGameplayTask_PlayContextualAnim, SectionIdx, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGameplayTask_PlayContextualAnim, AnimSetIdx, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGameplayTask_PlayContextualAnim, Pivots, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGameplayTask_PlayContextualAnim, InteractorRole, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGameplayTask_PlayContextualAnim, InteractableObject, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGameplayTask_PlayContextualAnim, InteractableObjectRole, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGameplayTask_PlayContextualAnim, SceneAsset, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGameplayTask_PlayContextualAnim, ExitSectionName, COND_InitialOnly);
}

void UGameplayTask_PlayContextualAnim::OnSectionEndTimeReached(UContextualAnimSceneInstance*)
{
	UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Received OnSectionEndTimeReached %s: marked task as 'DonePlaying'"), *GetNameSafe(this));
	Status = EPlayContextualAnimStatus::DonePlaying;

	OnTransitionCompleted.Broadcast(EGameplayTransitionResult::Succeeded, this);

	OnCompleted.Broadcast(EGameplayTaskActuationResult::Succeeded, GetAvatarActor());
}

void UGameplayTask_PlayContextualAnim::SharedInitAndApply()
{
	Status = EPlayContextualAnimStatus::Failed;

	if (SceneAsset == nullptr)
	{
		UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s failed: missing scene asset."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	AActor* Interactor = GetAvatarActor();
	if (Interactor == nullptr)
	{
		UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s failed: missing interactor actor."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if (SectionIdx == INDEX_NONE || AnimSetIdx == INDEX_NONE)
	{
		UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s failed: unspecified Section or AnimSet index."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	// Configure FContextualAnimStartSceneParams based on initial replicated data
	SceneParams.SectionIdx = SectionIdx;
	SceneParams.AnimSetIdx = AnimSetIdx;
	SceneParams.RoleToActorMap.Add(InteractorRole, Interactor);
	SceneParams.RoleToActorMap.Add(InteractableObjectRole, ToRawPtr(InteractableObject));

	// Rebuild pivots from asset and replicated transforms
	if (Pivots.Num())
	{
		const TArray<FContextualAnimSetPivotDefinition>& PivotDefinitions = SceneAsset->GetAnimSetPivotDefinitionsInSection(SectionIdx);
		if (ensureMsgf(Pivots.Num() == PivotDefinitions.Num(), TEXT("Number of provided pivots is expected to match number of pivot definitions")))
		{
			TArray<FContextualAnimSetPivot> AnimSetPivots;
			AnimSetPivots.Reserve(Pivots.Num());
			for (int32 Index = 0; Index < PivotDefinitions.Num(); Index++)
			{
				AnimSetPivots.Emplace(PivotDefinitions[Index].Name, Pivots[Index]);
			}
			SceneParams.Pivots = AnimSetPivots;
		}
	}

	const FGameplayActuationState_ContextualAnim* CurrentState = nullptr;
	ActuationComponent = Interactor->FindComponentByClass<UGameplayActuationComponent>();
	if (ActuationComponent != nullptr)
	{
		// Carry scene from previous state if possible.
		UContextualAnimSceneInstance* ExistingSceneInstance = nullptr;
		CurrentState = ActuationComponent->GetActuationState().GetPtr<FGameplayActuationState_ContextualAnim>();
		if (CurrentState != nullptr
			&& CurrentState->SceneAsset == SceneAsset
			&& CurrentState->InteractableObject == InteractableObject)
		{
			ExistingSceneInstance = CurrentState->SceneInstance;
		}

		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Log, TEXT("PlayContextualAnim: Existing scene instance: %s"), *GetNameSafe(ExistingSceneInstance));
		SceneInstance = ExistingSceneInstance;
	}
	else
	{
		UE_VLOG_UELOG(Interactor, LogGameplayTasks, Error, TEXT("%s failed: missing UGameplayActuationComponent."), ANSI_TO_TCHAR(__FUNCTION__));
	}

	ActuationState.InteractorRole = InteractorRole;
	ActuationState.InteractableObject = InteractableObject;
	ActuationState.InteractableObjectRole = InteractableObjectRole;
	TArray<FName> Names = SceneAsset->GetSectionNames();
	ActuationState.SectionName = Names.IsValidIndex(SectionIdx) ? Names[SectionIdx] : NAME_None;
	ActuationState.ExitSectionName = ExitSectionName;
	ActuationState.SceneAsset = SceneAsset;
	ActuationState.SceneInstance = SceneInstance;

	// Log after updating all parameters so vislog snapshot will contain latest values
	UE_CVLOG_UELOG(CurrentState != nullptr, ActuationComponent, LogGameplayTasks, Log, TEXT("ContextualAnim actuation state updated: '%s'."), *GetNameSafe(this));

	TransitionToSection();
}

void UGameplayTask_PlayContextualAnim::OnDestroy(const bool bInOwnerFinished)
{
	if (SceneInstance != nullptr)
	{
		SceneInstance->OnSectionEndTimeReached.RemoveDynamic(this, &UGameplayTask_PlayContextualAnim::OnSectionEndTimeReached);
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Remove OnSectionEndTimeReached: %s"), *GetNameSafe(this));
	}

	if (bTeleportOnTaskEnd)
	{
		AActor* Interactor = GetAvatarActor();
		if (Interactor != nullptr && Interactor->TeleportTo(SafeExitPoint.GetLocation(), SafeExitPoint.GetRotation().Rotator()))
		{
			UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Teleport succeeded for actor: %s"), *GetNameSafe(Interactor));
		}
		else
		{
			UE_VLOG_UELOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("Teleport failed for actor: %s"), *GetNameSafe(Interactor));
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}

FString UGameplayTask_PlayContextualAnim::GetDebugString() const
{
	TStringBuilder<256> MappingsString;
	MappingsString.Appendf(TEXT("%s: %s, %s: %s"), *InteractorRole.ToString(), *GetNameSafe(GetAvatarActor()), *InteractableObjectRole.ToString(), *GetNameSafe(InteractableObject));

	return FString::Printf(TEXT("PlayContextualAnim: Asset '%s' - Section '%d' - AnimSet '%d' - [%s]")
		, *GetNameSafe(SceneAsset)
		, SceneParams.SectionIdx
		, SceneParams.AnimSetIdx
		, *MappingsString
		);
}

bool UGameplayTask_PlayContextualAnim::CreateBindings(const UContextualAnimSceneAsset& SceneAsset, const FContextualAnimStartSceneParams& SceneParams, FContextualAnimSceneBindings& OutBindings)
{
	const int32 SectionIdx = SceneParams.SectionIdx;
	if (SceneParams.AnimSetIdx != INDEX_NONE)
	{
		const int32 AnimSetIdx = SceneParams.AnimSetIdx;
		// Try to find matching bindings (using selection criteria)
		if (!FContextualAnimSceneBindings::TryCreateBindings(SceneAsset, SectionIdx, AnimSetIdx, SceneParams.RoleToActorMap, OutBindings))
		{
			OutBindings = FContextualAnimSceneBindings(SceneAsset, SectionIdx, AnimSetIdx);

			// Force bindings to the first AnimSet (ignoring selection criteria)
			for (const auto& Pair : SceneParams.RoleToActorMap)
			{
				FName RoleToBind = Pair.Key;
				if (const FContextualAnimTrack* AnimTrack = SceneAsset.GetAnimTrack(SectionIdx, AnimSetIdx, RoleToBind))
				{
					OutBindings.Add(FContextualAnimSceneBinding(Pair.Value, *AnimTrack));
				}
			}
		}
	}
	else
	{
		// Try to find matching bindings (using selection criteria)
		const int32 NumAnimSets = SceneAsset.GetNumAnimSetsInSection(SectionIdx);
		for (int32 AnimSetIdx = 0; AnimSetIdx < NumAnimSets; AnimSetIdx++)
		{
			if (FContextualAnimSceneBindings::TryCreateBindings(SceneAsset, SectionIdx, AnimSetIdx, SceneParams.RoleToActorMap, OutBindings))
			{
				break;
			}
		}

		// Force bindings to the first AnimSet (ignoring selection criteria)
		if (OutBindings.Num() != SceneAsset.GetNumRoles() && NumAnimSets > 0)
		{
			for (int32 AnimSetIdx = 0; AnimSetIdx < NumAnimSets; AnimSetIdx++)
			{
				OutBindings = FContextualAnimSceneBindings(SceneAsset, SectionIdx, AnimSetIdx);

				for (const auto& Pair : SceneParams.RoleToActorMap)
				{
					FName RoleToBind = Pair.Key;
					if (const FContextualAnimTrack* AnimTrack = SceneAsset.GetAnimTrack(SectionIdx, AnimSetIdx, RoleToBind))
					{
						OutBindings.Add(FContextualAnimSceneBinding(Pair.Value, *AnimTrack));
					}
				}

				if (OutBindings.Num() == SceneParams.RoleToActorMap.Num())
				{
					break;
				}
			}
		}
	}

	return (OutBindings.Num() == SceneParams.RoleToActorMap.Num());
}

#if ENABLE_VISUAL_LOG
void UGameplayTask_PlayContextualAnim::TickTask(float DeltaTime)
{
	if (const AActor* Interactor = GetAvatarActor())
	{
		const FVector CurrentLocation = Interactor->GetActorLocation();
		UE_VLOG_LOCATION(GetGameplayTasksComponent(), LogGameplayTasks, Display, CurrentLocation, 30, FColor::White, TEXT("Play CA"));
	}
}
#endif

