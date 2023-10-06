// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTask_PlayContextualAnim.h"

#include "ContextualAnimSceneAsset.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "ContextualAnimUtilities.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_PlayContextualAnim)

//-----------------------------------------------------
// FStateTreeTask_PlayContextualAnim
//-----------------------------------------------------

FStateTreeTask_PlayContextualAnim::FStateTreeTask_PlayContextualAnim()
{
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreeTask_PlayContextualAnim::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UInstanceDataType* InstanceData = Context.GetInstanceDataPtr<UInstanceDataType>(*this);
	check(InstanceData);

	return InstanceData->OnEnterState(Context);
}

void FStateTreeTask_PlayContextualAnim::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UInstanceDataType* InstanceData = Context.GetInstanceDataPtr<UInstanceDataType>(*this);
	check(InstanceData);

	InstanceData->OnExitState();
}

EStateTreeRunStatus FStateTreeTask_PlayContextualAnim::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UInstanceDataType* InstanceData = Context.GetInstanceDataPtr<UInstanceDataType>(*this);
	check(InstanceData);

	return InstanceData->OnTick(Context, DeltaTime);
}

//-----------------------------------------------------
// UStateTreeTask_PlayContextualAnim_InstanceData
//-----------------------------------------------------

EStateTreeRunStatus UStateTreeTask_PlayContextualAnim_InstanceData::OnEnterState(const FStateTreeExecutionContext& Context)
{
	if (SceneAsset == nullptr || !SceneAsset->HasValidData())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Invalid Scene Asset"), __FUNCTION__);
		return EStateTreeRunStatus::Failed;
	}

	// PrimaryActor is mandatory, this would be the interactable object and will be always bound to the Primary role in the SceneAsset
	if (PrimaryActor == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Invalid PrimaryActor"), __FUNCTION__);
		return EStateTreeRunStatus::Failed;
	}

	// Secondary actor is also mandatory (for this type of interactions to make sense we need at least two actors). Role needs to be explicitly defined
	if (SecondaryActor == nullptr || SecondaryRole == NAME_None)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Invalid SecondaryActor (%s) or Role (%s)"),
			__FUNCTION__, *GetNameSafe(SecondaryActor), *SecondaryRole.ToString());
		return EStateTreeRunStatus::Failed;
	}

	UAnimInstance* AnimInstance = UContextualAnimUtilities::TryGetAnimInstance(SecondaryActor);
	if (AnimInstance == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Invalid AnimInstance. Actor: %s"),
			__FUNCTION__, *GetNameSafe(SecondaryActor));
		return EStateTreeRunStatus::Failed;
	}

	const EStateTreeRunStatus Result = Play(Context);

	if (Result == EStateTreeRunStatus::Failed)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Failed to play the interaction"), __FUNCTION__);
		return EStateTreeRunStatus::Failed;
	}
	
	UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Log, TEXT("%hs: %s - %s"), __FUNCTION__, *GetName(), *UEnum::GetValueAsString(Result));

	AnimInstance->OnMontageBlendingOut.AddUniqueDynamic(this, &UStateTreeTask_PlayContextualAnim_InstanceData::OnMontageEnded);
	AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UStateTreeTask_PlayContextualAnim_InstanceData::OnNotifyBeginReceived);

	return EStateTreeRunStatus::Running;
}

bool UStateTreeTask_PlayContextualAnim_InstanceData::StartContextualAnim(const FStateTreeExecutionContext& Context) const
{
	int32 SectionIdx = SceneAsset->GetSectionIndex(SectionName);
	if (SectionIdx == INDEX_NONE)
	{
		SectionIdx = 0;
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs. '%s' is not a valid section name in %s. Falling back to first section!"),
			__FUNCTION__, *SectionName.ToString(), *GetNameSafe(SceneAsset));
	}

	// Randomly select the set to play if there is more than one for the given section
	// @TODO: Temporarily here until we move it to the contextual anim plugin as part of the selection mechanism. 

	int32 AnimSetIdx = 0;
	const int32 NumSets = SceneAsset->GetNumAnimSetsInSection(SectionIdx);
	if (NumSets > 1)
	{
		const FContextualAnimSceneSection* Section = SceneAsset->GetSection(SectionIdx);

		int32 TotalWeight = 0;
		for (int32 Idx = 0; Idx < NumSets; Idx++)
		{
			TotalWeight += FMath::Max(Section->GetAnimSet(Idx)->RandomWeight, 0);
		}

		int32 RandomValue = FMath::RandRange(0, TotalWeight);
		for (int32 Idx = 0; Idx < NumSets; Idx++)
		{
			RandomValue -= FMath::Max(Section->GetAnimSet(Idx)->RandomWeight, 0);
			if (RandomValue <= 0)
			{
				AnimSetIdx = Idx;
				break;
			}
		}
	}

	const FName PrimaryRole = SceneAsset->GetPrimaryRole();
	FContextualAnimSceneBindings Bindings = FContextualAnimSceneBindings(*SceneAsset, SectionIdx, AnimSetIdx);

	// Add primary actor to the contextual anim scene bindings
	if (Bindings.BindActorToRole(*PrimaryActor, PrimaryRole) == false)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Failed to bind PrimaryActor"), __FUNCTION__);
		return false;
	}

	// Add secondary actor to the contextual anim scene bindings
	if (Bindings.BindActorToRole(*SecondaryActor, SecondaryRole) == false)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Failed to bind SecondaryActor"), __FUNCTION__);
		return false;
	}

	// If another actor is defined and a valid role is set, add it to the contextual anim scene bindings too
	// @TODO: This will be replaced by a loop over a dynamic array once we have that
	if (TertiaryActor && TertiaryRole != NAME_None)
	{
		if (Bindings.BindActorToRole(*TertiaryActor, TertiaryRole) == false)
		{
			UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Invalid TertiaryActor (%s) or Role (%s)"),
				__FUNCTION__, *GetNameSafe(TertiaryActor), *TertiaryRole.ToString());
			return false;
		}
	}

	// Ensure that Bindings are valid after adding the actors
	if (!Bindings.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Invalid Bindings."), __FUNCTION__);
		return false;
	}

	// Bump up unique id on each loop to ensure replication when all the relevant data in the bindings is the same
	// @TODO: Temp here until we move it to the comp
	Bindings.GenerateUniqueId();

	UContextualAnimSceneActorComponent* SceneActorComp = SecondaryActor->FindComponentByClass<UContextualAnimSceneActorComponent>();
	check(SceneActorComp);

	SceneActorComp->StartContextualAnimScene(Bindings, WarpTargets);

	return true;
}

bool UStateTreeTask_PlayContextualAnim_InstanceData::JoinContextualAnim(const FStateTreeExecutionContext& Context) const
{
	UContextualAnimSceneActorComponent* SceneActorComp = PrimaryActor->FindComponentByClass<UContextualAnimSceneActorComponent>();
	if (SceneActorComp == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Missing SceneActorComp on PrimaryActor"), __FUNCTION__);
		return false;
	}

	const bool bResult = SceneActorComp->LateJoinContextualAnimScene(SecondaryActor, SecondaryRole, WarpTargets);
	UE_CVLOG_UELOG(!bResult, Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: LateJoinContextualAnimScene Failed"), __FUNCTION__);

	return bResult;
}

bool UStateTreeTask_PlayContextualAnim_InstanceData::TransitionSingleActor(const FStateTreeExecutionContext& Context) const
{
	UContextualAnimSceneActorComponent* SceneActorComp = SecondaryActor->FindComponentByClass<UContextualAnimSceneActorComponent>();
	if (SceneActorComp == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Missing SceneActorComp on SecondaryActor"), __FUNCTION__);
		return false;
	}

	const UContextualAnimSceneAsset* Asset = SceneActorComp->GetBindings().GetSceneAsset();
	if (Asset == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Invalid SceneAsset"), __FUNCTION__);
		return false;
	}

	const int32 SectionIdx = Asset->GetSectionIndex(SectionName);
	if (SectionIdx == INDEX_NONE)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: '%s' is not a valid section name in %s"),
			__FUNCTION__, *SectionName.ToString(), *GetNameSafe(Asset));
		return false;
	}

	// For now we always transition to the first AnimSet in the section. 
	// We may want to change that if we end up having multiple animations (variations) for a non primary section but keeping it simple for now.
	const bool bResult = SceneActorComp->TransitionSingleActor(SectionIdx, 0, WarpTargets);
	UE_CVLOG_UELOG(!bResult, Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed."), __FUNCTION__);

	return bResult;
}

bool UStateTreeTask_PlayContextualAnim_InstanceData::TransitionAllActors(const FStateTreeExecutionContext& Context) const
{
	UContextualAnimSceneActorComponent* SceneActorComp = SecondaryActor->FindComponentByClass<UContextualAnimSceneActorComponent>();
	if (SceneActorComp == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: Missing SceneActorComp on SecondaryActor"), __FUNCTION__);
		return false;
	}

	const bool bResult = SceneActorComp->TransitionContextualAnimScene(SectionName, WarpTargets);
	UE_CVLOG_UELOG(!bResult, Context.GetOwner(), LogStateTree, Warning, TEXT("%hs Failed. Reason: TransitionContextualAnimScene Failed"), __FUNCTION__);

	return bResult;
}

void UStateTreeTask_PlayContextualAnim_InstanceData::OnExitState()
{
	CleanUp();
}

EStateTreeRunStatus UStateTreeTask_PlayContextualAnim_InstanceData::OnTick(const FStateTreeExecutionContext& Context, float DeltaTime)
{
	if (bMontageInterrupted)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!bLoopForever && CompletedLoops >= LoopsToRun)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// If it's not set we are running an animation, otherwise we need to run one or are waiting on the delay
	EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Running;
	if (TimeBeforeStartingNewLoop.IsSet())
	{
		*TimeBeforeStartingNewLoop -= DeltaTime;
		if (*TimeBeforeStartingNewLoop <= 0.f)
		{
			RunStatus = Play(Context);
		}
	}

	return RunStatus;
}

EStateTreeRunStatus UStateTreeTask_PlayContextualAnim_InstanceData::Play(const FStateTreeExecutionContext& Context)
{
	// Need to unset so that no new animation loop will be run by the tick!
	TimeBeforeStartingNewLoop.Reset();

	bool bResult = false;
	switch (ExecutionMethod)
	{
	case EPlayContextualAnimExecutionMethod::StartInteraction:
		bResult = StartContextualAnim(Context);
		break;
	case EPlayContextualAnimExecutionMethod::JoinInteraction:
		bResult = JoinContextualAnim(Context);
		break;
	case EPlayContextualAnimExecutionMethod::TransitionAllActors:
		bResult = TransitionAllActors(Context);
		break;
	case EPlayContextualAnimExecutionMethod::TransitionSingleActor:
		bResult = TransitionSingleActor(Context);
		break;
	default: check(false); break;
	}

	return bResult ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void UStateTreeTask_PlayContextualAnim_InstanceData::CleanUp()
{
	CompletedLoops = 0;
	bMontageInterrupted = false;
	bExpectInterruptionFromNotifyEventLoop = false;
	TimeBeforeStartingNewLoop.Reset();

	if (UAnimInstance* AnimInstance = UContextualAnimUtilities::TryGetAnimInstance(SecondaryActor))
	{
		AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &UStateTreeTask_PlayContextualAnim_InstanceData::OnMontageEnded);
		AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UStateTreeTask_PlayContextualAnim_InstanceData::OnNotifyBeginReceived);
	}
}

void UStateTreeTask_PlayContextualAnim_InstanceData::OnMontageEnded(UAnimMontage* const EndedMontage, const bool bInterrupted)
{
	// Handling of specific case: when we forcibly start a new montage from a notify event,
	// an interruption will happen from the previous event, so we need to make sure to ignore the first occurrence
	if (bWaitForNotifyEventToEnd && bInterrupted && bExpectInterruptionFromNotifyEventLoop)
	{
		bExpectInterruptionFromNotifyEventLoop = false;

		return;
	}

	// Ignore OnMontageEnd event if we are waiting for a notify to signal th end event, unless the montage was interrupted, in which case we should end immediately.
	if (bWaitForNotifyEventToEnd && !bInterrupted)
	{
		return;
	}

	UE_LOG(LogStateTree, Verbose, TEXT("%hs EndedMontage: %s bInterrupted: %d"), __FUNCTION__, *GetNameSafe(EndedMontage), bInterrupted);

	if (bInterrupted)
	{
		bMontageInterrupted = true;
	}

	CompletedLoops++;
	TimeBeforeStartingNewLoop = FMath::FRandRange(FMath::Max(0.0f, DelayBetweenLoops - RandomDeviationBetweenLoops), (DelayBetweenLoops + RandomDeviationBetweenLoops));
}

void UStateTreeTask_PlayContextualAnim_InstanceData::OnNotifyBeginReceived(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
	UE_LOG(LogStateTree, Verbose, TEXT("%hs NotifyName: %s Anim: %s"),	__FUNCTION__, *NotifyName.ToString(), *GetNameSafe(BranchingPointNotifyPayload.SequenceAsset));

	// Increment the completed counter, if we are waiting for notify event to end and we receive the event with the expected notify name
	if (bWaitForNotifyEventToEnd && NotifyEventNameToEnd == NotifyName)
	{
		bExpectInterruptionFromNotifyEventLoop = true;

		CompletedLoops++;
		TimeBeforeStartingNewLoop = FMath::FRandRange(FMath::Max(0.0f, DelayBetweenLoops - RandomDeviationBetweenLoops), (DelayBetweenLoops + RandomDeviationBetweenLoops));
	}
}