// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneSkeletalAnimationSystem.h"

#include "Async/TaskGraphInterfaces.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneExecutionToken.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"

#include "Evaluation/PreAnimatedState/IMovieScenePreAnimatedStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedObjectStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"

#include "AnimCustomInstanceHelper.h"
#include "AnimSequencerInstance.h"
#include "AnimSequencerInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "SkeletalMeshRestoreState.h"

#include "Rendering/MotionVectorSimulation.h"
#include "Systems/MovieSceneMotionVectorSimulationSystem.h"
#include "Systems/MovieSceneComponentTransformSystem.h"
#include "Systems/MovieSceneQuaternionInterpolationRotationSystem.h"
#include "Systems/WeightAndEasingEvaluatorSystem.h"

#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSkeletalAnimationSystem)

DECLARE_CYCLE_STAT(TEXT("Gather skeletal animations"), MovieSceneEval_GatherSkeletalAnimations, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("Evaluate skeletal animations"), MovieSceneEval_EvaluateSkeletalAnimations, STATGROUP_MovieSceneECS);

namespace UE::MovieScene
{

/** Helper function to get our sequencer animation node from a skeletal mesh component */
UAnimSequencerInstance* GetAnimSequencerInstance(USkeletalMeshComponent* SkeletalMeshComponent)
{
	ISequencerAnimationSupport* SeqInterface = Cast<ISequencerAnimationSupport>(SkeletalMeshComponent->GetAnimInstance());
	if (SeqInterface)
	{
		return Cast<UAnimSequencerInstance>(SeqInterface->GetSourceAnimInstance());
	}

	return nullptr;
}

/** ------------------------------------------------------------------------- */

/** Pre-animated state for skeletal animations */
struct FPreAnimatedSkeletalAnimationState
{
	EAnimationMode::Type AnimationMode;
	TStrongObjectPtr<UAnimInstance> CachedAnimInstance;
	FSkeletalMeshRestoreState SkeletalMeshRestoreState;
};

/** Pre-animation traits for skeletal animations */
struct FPreAnimatedSkeletalAnimationTraits : FBoundObjectPreAnimatedStateTraits
{
	using KeyType     = FObjectKey;
	using StorageType = FPreAnimatedSkeletalAnimationState;

	FPreAnimatedSkeletalAnimationState CachePreAnimatedValue(const KeyType& Object)
	{
		FPreAnimatedSkeletalAnimationState OutCachedValue;
		USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(Object.ResolveObjectPtr());
		if (ensure(Component))
		{
			OutCachedValue.AnimationMode = Component->GetAnimationMode();
			OutCachedValue.CachedAnimInstance.Reset(Component->AnimScriptInstance);
			OutCachedValue.SkeletalMeshRestoreState.SaveState(Component);
		}
		return OutCachedValue;
	}

	void RestorePreAnimatedValue(const KeyType& Object, StorageType& InOutCachedValue, const FRestoreStateParams& Params)
	{
		USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(Object.ResolveObjectPtr());
		if (!Component)
		{
			return;
		}

		ISequencerAnimationSupport* SequencerInst = Cast<ISequencerAnimationSupport>(GetAnimSequencerInstance(Component));
		if (SequencerInst)
		{
			SequencerInst->ResetPose();
			SequencerInst->ResetNodes();
		}

		FAnimCustomInstanceHelper::UnbindFromSkeletalMeshComponent<UAnimSequencerInstance>(Component);

		// Restore LOD before reinitializing anim instance
		InOutCachedValue.SkeletalMeshRestoreState.RestoreLOD(Component);

		if (Component->GetAnimationMode() != InOutCachedValue.AnimationMode)
		{
			// this SetAnimationMode reinitializes even if the mode is same
			// if we're using same anim blueprint, we don't want to keep reinitializing it. 
			Component->SetAnimationMode(InOutCachedValue.AnimationMode);
		}
		if (InOutCachedValue.CachedAnimInstance.Get())
		{
			Component->AnimScriptInstance = InOutCachedValue.CachedAnimInstance.Get();
			InOutCachedValue.CachedAnimInstance.Reset();
			if (Component->AnimScriptInstance && Component->GetSkeletalMeshAsset() && Component->AnimScriptInstance->CurrentSkeleton != Component->GetSkeletalMeshAsset()->GetSkeleton())
			{
				//the skeleton may have changed so need to recalc required bones as needed.
				Component->AnimScriptInstance->CurrentSkeleton = Component->GetSkeletalMeshAsset()->GetSkeleton();
				//Need at least RecalcRequiredbones and UpdateMorphTargetrs
				Component->InitializeAnimScriptInstance(true);
			}
		}

		// Restore pose after unbinding to force the restored pose
		Component->SetUpdateAnimationInEditor(true);
		Component->SetUpdateClothInEditor(true);
		Component->TickAnimation(0.f, false);

		Component->RefreshBoneTransforms();
		Component->RefreshFollowerComponents();
		Component->UpdateComponentToWorld();
		Component->FinalizeBoneTransform();
		Component->MarkRenderTransformDirty();
		Component->MarkRenderDynamicDataDirty();

		// Reset the mesh component update flag and animation mode to what they were before we animated the object
		InOutCachedValue.SkeletalMeshRestoreState.RestoreState(Component);

		// if not game world, don't clean this up
		if (Component->GetWorld() != nullptr && Component->GetWorld()->IsGameWorld() == false)
		{
			Component->ClearMotionVector();
		}
	}
};

/** Pre-animation storage for skeletal animations */
struct FPreAnimatedSkeletalAnimationStorage : TPreAnimatedStateStorage_ObjectTraits<FPreAnimatedSkeletalAnimationTraits>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedSkeletalAnimationStorage> StorageID;
};

TAutoRegisterPreAnimatedStorageID<FPreAnimatedSkeletalAnimationStorage> FPreAnimatedSkeletalAnimationStorage::StorageID;

/** ------------------------------------------------------------------------- */

/** Pre-animated state for a sequencer montage node */
struct FPreAnimatedSkeletalAnimationMontageState
{
	TWeakObjectPtr<UAnimInstance> WeakInstance;
	int32 MontageInstanceId;
};

/** Pre-animated traits for a sequencer montage node */
struct FPreAnimatedSkeletalAnimationMontageTraits : FBoundObjectPreAnimatedStateTraits
{
	using KeyType     = FObjectKey;
	using StorageType = FPreAnimatedSkeletalAnimationMontageState;

	FPreAnimatedSkeletalAnimationMontageState CachePreAnimatedValue(const FObjectKey& Object)
	{
		// Should be unused, as we always cache state with captured values.
		return FPreAnimatedSkeletalAnimationMontageState();
	}

	void RestorePreAnimatedValue(const FObjectKey& Object, FPreAnimatedSkeletalAnimationMontageState& InOutCachedValue, const FRestoreStateParams& Params)
	{
		UAnimInstance* AnimInstance = InOutCachedValue.WeakInstance.Get();
		if (AnimInstance)
		{
			FAnimMontageInstance* MontageInstance = AnimInstance->GetMontageInstanceForID(InOutCachedValue.MontageInstanceId);
			if (MontageInstance)
			{
				MontageInstance->Stop(FAlphaBlend(0.f), false);
			}
		}
	}
};

/** Pre-animated storage for a sequencer montage node */
struct FPreAnimatedSkeletalAnimationMontageStorage : TPreAnimatedStateStorage_ObjectTraits<FPreAnimatedSkeletalAnimationMontageTraits>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedSkeletalAnimationMontageStorage> StorageID;
};

TAutoRegisterPreAnimatedStorageID<FPreAnimatedSkeletalAnimationMontageStorage> FPreAnimatedSkeletalAnimationMontageStorage::StorageID;

/** ------------------------------------------------------------------------- */

/** Pre-animated traits for a sequencer animation node */
struct FPreAnimatedSkeletalAnimationAnimInstanceTraits : FBoundObjectPreAnimatedStateTraits
{
	using KeyType     = FObjectKey;
	using StorageType = bool; // We actually don't need any state, so this will be a dummy value

	bool CachePreAnimatedValue(const FObjectKey& Object)
	{
		// Nothing to do, we just need the object pointer to restore state.
		return true;
	}

	void RestorePreAnimatedValue(const FObjectKey& Object, bool& Unused, const FRestoreStateParams& Params)
	{
		if (UObject* ObjectPtr = Object.ResolveObjectPtr())
		{
			ISequencerAnimationSupport* SequencerAnimationSupport = Cast<ISequencerAnimationSupport>(ObjectPtr);
			if (ensure(SequencerAnimationSupport))
			{
				SequencerAnimationSupport->ResetNodes();
			}
		}
	}
};

/** Pre-animated storage for a sequencer animation node */
struct FPreAnimatedSkeletalAnimationAnimInstanceStorage : TPreAnimatedStateStorage_ObjectTraits<FPreAnimatedSkeletalAnimationAnimInstanceTraits>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedSkeletalAnimationAnimInstanceStorage> StorageID;
};

TAutoRegisterPreAnimatedStorageID<FPreAnimatedSkeletalAnimationAnimInstanceStorage> FPreAnimatedSkeletalAnimationAnimInstanceStorage::StorageID;

/** ------------------------------------------------------------------------- */

/** Task for gathering active skeletal animations */
struct FGatherSkeletalAnimations
{
	const FInstanceRegistry* InstanceRegistry;
	FSkeletalAnimationSystemData& SystemData;

	FGatherSkeletalAnimations(const FInstanceRegistry* InInstanceRegistry, FSkeletalAnimationSystemData& InSystemData)
		: InstanceRegistry(InInstanceRegistry)
		, SystemData(InSystemData)
	{
	}

	void ForEachAllocation(
			const FEntityAllocationProxy AllocationProxy, 
			TRead<FMovieSceneEntityID> EntityIDs,
			TRead<FRootInstanceHandle> RootInstanceHandles,
			TRead<FInstanceHandle> InstanceHandles,
			TRead<UObject*> BoundObjects,
			TRead<FMovieSceneSkeletalAnimationComponentData> SkeletalAnimations,
			TReadOptional<double> WeightAndEasings)
	{
		// Gather all the skeletal animations currently active in all sequences.
		// We map these animations to their bound object, which means we might blend animations from different sequences
		// that have bound to the same object.
		const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		const FEntityAllocation* Allocation = AllocationProxy.GetAllocation();
		const int32 Num = Allocation->Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			FMovieSceneEntityID EntityID(EntityIDs[Index]);
			const FRootInstanceHandle& RootInstanceHandle(RootInstanceHandles[Index]);
			const FInstanceHandle& InstanceHandle(InstanceHandles[Index]);
			UObject* BoundObject(BoundObjects[Index]);
			const FMovieSceneSkeletalAnimationComponentData& SkeletalAnimation(SkeletalAnimations[Index]);
			const double Weight = (WeightAndEasings ? WeightAndEasings[Index] : 1.f);

			const bool bWantsRestoreState = AllocationProxy.GetAllocationType().Contains(BuiltInComponents->Tags.RestoreState);

			// Get the full context, so we can get both the current and previous evaluation times.
			const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InstanceHandle);
			const FMovieSceneContext& Context = SequenceInstance.GetContext();
			IMovieScenePlayer* Player = SequenceInstance.GetPlayer();

			// Calculate the time at which to evaluate the animation
			const UMovieSceneSkeletalAnimationSection* AnimSection = SkeletalAnimation.Section;
			const FMovieSceneSkeletalAnimationParams& AnimParams = AnimSection->Params;
			if (AnimParams.Animation == nullptr)
			{
				continue;
			}
			const float EvalTime = AnimParams.MapTimeToAnimation(AnimSection, Context.GetTime(), Context.GetFrameRate());
			const float PreviousEvalTime = AnimParams.MapTimeToAnimation(AnimSection, Context.GetPreviousTime(), Context.GetFrameRate());

			FBoundObjectActiveSkeletalAnimations& BoundObjectAnimations = SystemData.SkeletalAnimations.FindOrAdd(BoundObject);
			BoundObjectAnimations.Animations.Add(FActiveSkeletalAnimation{ Player, AnimSection, Context, EntityID, RootInstanceHandle, PreviousEvalTime, EvalTime, Weight, bWantsRestoreState });

			if (FMotionVectorSimulation::IsEnabled())
			{
				const FFrameTime SimulatedTime = UE::MovieScene::GetSimulatedMotionVectorTime(Context);

				// Calculate the time at which to evaluate the animation
				const float SimulatedEvalTime = AnimParams.MapTimeToAnimation(AnimSection, SimulatedTime, Context.GetFrameRate());

				// Evaluate the weight channel and section easing at the simulation time... right now we don't benefit
				// from that being evaluated by the channel evaluators.
				float SimulatedManualWeight = 1.f;
				AnimParams.Weight.Evaluate(SimulatedTime, SimulatedManualWeight);

				const float SimulatedWeight = SimulatedManualWeight * AnimSection->EvaluateEasing(SimulatedTime);

				BoundObjectAnimations.SimulatedAnimations.Add(FActiveSkeletalAnimation{ Player, AnimSection, Context, EntityID, RootInstanceHandle, EvalTime, SimulatedEvalTime, SimulatedWeight, bWantsRestoreState });
			}
		}
	}
};

/** Task for evaluating skeletal animations */
struct FEvaluateSkeletalAnimations
{
private:

	UMovieSceneEntitySystemLinker* Linker;
	FSkeletalAnimationSystemData& SystemData;

	TSharedPtr<FPreAnimatedSkeletalAnimationStorage> PreAnimatedStorage;
	TSharedPtr<FPreAnimatedSkeletalAnimationMontageStorage> PreAnimatedMontageStorage;
	TSharedPtr<FPreAnimatedSkeletalAnimationAnimInstanceStorage> PreAnimatedAnimInstanceStorage;

public:

	FEvaluateSkeletalAnimations(UMovieSceneEntitySystemLinker* InLinker, FSkeletalAnimationSystemData& InSystemData)
		: Linker(InLinker)
		, SystemData(InSystemData)
	{
		PreAnimatedStorage = InLinker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedSkeletalAnimationStorage>();
		PreAnimatedMontageStorage = InLinker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedSkeletalAnimationMontageStorage>();
		PreAnimatedAnimInstanceStorage = InLinker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedSkeletalAnimationAnimInstanceStorage>();
	}

	void Run()
	{
		for (const TTuple<UObject*, FBoundObjectActiveSkeletalAnimations>& Pair : SystemData.SkeletalAnimations)
		{
			EvaluateSkeletalAnimations(Pair.Key, Pair.Value);
		}
	}

private:

	void EvaluateSkeletalAnimations(UObject* InObject, const FBoundObjectActiveSkeletalAnimations& InSkeletalAnimations)
	{
		ensureMsgf(InObject, TEXT("Attempting to evaluate an Animation track with a null object."));

		// Get the bound skeletal mesh component.
		USkeletalMeshComponent* SkeletalMeshComponent = SkeletalMeshComponentFromObject(InObject);
		if (!SkeletalMeshComponent || !SkeletalMeshComponent->GetSkeletalMeshAsset())
		{
			return;
		}

		// Cache pre-animated state for this bound object before doing anything.
		// We don't yet track what entities have already started animated vs. entities that just started this frame,
		// so we just process all the currently active ones. If they are already tracked and have already had their
		// pre-animated state saved, it these calls will just early return.
		for (const FActiveSkeletalAnimation& SkeletalAnimation : InSkeletalAnimations.Animations)
		{
			PreAnimatedStorage->BeginTrackingEntity(SkeletalAnimation.EntityID, SkeletalAnimation.bWantsRestoreState, SkeletalAnimation.RootInstanceHandle, SkeletalMeshComponent);
		}
		FCachePreAnimatedValueParams CacheParams;
		PreAnimatedStorage->CachePreAnimatedValue(CacheParams, SkeletalMeshComponent);

		// Setup any needed animation nodes for sequencer playback.
		UAnimInstance* ExistingAnimInstance = GetSourceAnimInstance(SkeletalMeshComponent);
		bool bWasCreated = false;
		ISequencerAnimationSupport* SequencerInstance = FAnimCustomInstanceHelper::BindToSkeletalMeshComponent<UAnimSequencerInstance>(SkeletalMeshComponent, bWasCreated);
		if (SequencerInstance)
		{
			if (bWasCreated)
			{
				SequencerInstance->SavePose();
			}
			else
			{
				SequencerInstance->ConstructNodes();
			}
		}

		// Group skeletal animations by player, in case we have multiple ongoing sequences binding to the same object.
		TMap<IMovieScenePlayer*, FBoundObjectActiveSkeletalAnimations> SkeletalAnimationsPerPlayer;
		for (const FActiveSkeletalAnimation& Animation : InSkeletalAnimations.Animations)
		{
			FBoundObjectActiveSkeletalAnimations& SkeletalAnimationsForPlayer = SkeletalAnimationsPerPlayer.FindOrAdd(Animation.Player);
			SkeletalAnimationsForPlayer.Animations.Add(Animation);
		}
		for (const FActiveSkeletalAnimation& SimulatedAnimation : InSkeletalAnimations.SimulatedAnimations)
		{
			FBoundObjectActiveSkeletalAnimations& SkeletalAnimationsForPlayer = SkeletalAnimationsPerPlayer.FindOrAdd(SimulatedAnimation.Player);
			SkeletalAnimationsForPlayer.SimulatedAnimations.Add(SimulatedAnimation);
		}

		// Evaluate each group for the given bound object.
		for (const TTuple<IMovieScenePlayer*, FBoundObjectActiveSkeletalAnimations>& Pair : SkeletalAnimationsPerPlayer)
		{
			EvaluateSkeletalAnimationsForPlayer(*Pair.Key, SkeletalMeshComponent, ExistingAnimInstance, SequencerInstance, Pair.Value);
		}
	}

	void EvaluateSkeletalAnimationsForPlayer(
			IMovieScenePlayer& InPlayer, 
			USkeletalMeshComponent* InSkeletalMeshComponent, 
			UAnimInstance* InExistingAnimInstance, 
			ISequencerAnimationSupport* InSequencerInstance, 
			const FBoundObjectActiveSkeletalAnimations& InSkeletalAnimations)
	{
		const bool bPreviewPlayback = ShouldUsePreviewPlayback(InPlayer, *InSkeletalMeshComponent);

		const EMovieScenePlayerStatus::Type PlayerStatus = InPlayer.GetPlaybackStatus();

		// If the playback status is jumping, ie. one such occurrence is setting the time for thumbnail generation, disable anim notifies updates because it could fire audio.
		// If the playback status is scrubbing, we disable notifies for now because we can't properly fire them in all cases until we get evaluation range info.
		// We now layer this with the passed in notify toggle to force a disable in this case.
		const bool bFireNotifies = !bPreviewPlayback || (PlayerStatus != EMovieScenePlayerStatus::Jumping && PlayerStatus != EMovieScenePlayerStatus::Stopped && PlayerStatus != EMovieScenePlayerStatus::Scrubbing);

		// When jumping from one cut to another cut, the delta time should be 0 so that anim notifies before the current position are not evaluated. Note, anim notifies at the current time should still be evaluated.
		const FInstanceHandle RootInstanceHandle = InPlayer.GetEvaluationTemplate().GetRootInstanceHandle();
		const FSequenceInstance& RootInstance = Linker->GetInstanceRegistry()->GetInstance(RootInstanceHandle);
		const FMovieSceneContext& RootContext = RootInstance.GetContext();
		const double RootDeltaTime = (RootContext.HasJumped() ? FFrameTime(0) : RootContext.GetRange().Size<FFrameTime>() ) / RootContext.GetFrameRate();

		const bool bResetDynamics = PlayerStatus == EMovieScenePlayerStatus::Stepping || 
			PlayerStatus == EMovieScenePlayerStatus::Jumping || 
			PlayerStatus == EMovieScenePlayerStatus::Scrubbing || 
			(RootDeltaTime == 0.0f && PlayerStatus != EMovieScenePlayerStatus::Stopped);

		// Need to zero all weights first since we may be blending animation that are keeping state but are no longer active.

		if (InSequencerInstance)
		{
			InSequencerInstance->ResetNodes();
		}
		else if (InExistingAnimInstance)
		{
			for (const TPair<FObjectKey, FMontagePlayerPerSectionData >& Pair : SystemData.MontageData)
			{
				int32 InstanceId = Pair.Value.MontageInstanceId;
				FAnimMontageInstance* MontageInstanceToUpdate = InExistingAnimInstance->GetMontageInstanceForID(InstanceId);
				if (MontageInstanceToUpdate)
				{
					MontageInstanceToUpdate->SetDesiredWeight(0.0f);
					MontageInstanceToUpdate->SetWeight(0.0f);
				}
			}
		}

		if (InSkeletalAnimations.SimulatedAnimations.Num() != 0)
		{
			UMovieSceneMotionVectorSimulationSystem* MotionVectorSim = InPlayer.GetEvaluationTemplate().GetEntitySystemLinker()->FindSystem<UMovieSceneMotionVectorSimulationSystem>();
			if (MotionVectorSim && MotionVectorSim->IsSimulationEnabled())
			{
				ApplyAnimations(InPlayer, InSkeletalMeshComponent, InSkeletalAnimations.SimulatedAnimations, bPreviewPlayback, bFireNotifies, bResetDynamics);
				InSkeletalMeshComponent->TickAnimation(0.f, false);
				InSkeletalMeshComponent->ForceMotionVector();
		
				SimulateMotionVectors(InSkeletalMeshComponent, MotionVectorSim);
			}
		}

		ApplyAnimations(InPlayer, InSkeletalMeshComponent, InSkeletalAnimations.Animations, bPreviewPlayback, bFireNotifies, bResetDynamics);

		// If the skeletal component has already ticked this frame because tick prerequisites weren't set up yet or a new binding was created, forcibly tick this component to update.
		// This resolves first frame issues where the skeletal component ticks first, then the sequencer binding is resolved which sets up tick prerequisites
		// for the next frame.
		if (InSkeletalMeshComponent->PoseTickedThisFrame() || (InSequencerInstance && InSequencerInstance->GetSourceAnimInstance() != InExistingAnimInstance))
		{
			InSkeletalMeshComponent->TickAnimation(0.f, false);

			InSkeletalMeshComponent->RefreshBoneTransforms();
			InSkeletalMeshComponent->RefreshFollowerComponents();
			InSkeletalMeshComponent->UpdateComponentToWorld();
			InSkeletalMeshComponent->FinalizeBoneTransform();
			InSkeletalMeshComponent->MarkRenderTransformDirty();
			InSkeletalMeshComponent->MarkRenderDynamicDataDirty();
		}
	}

private:

	static bool ShouldUsePreviewPlayback(IMovieScenePlayer& Player, UObject& RuntimeObject)
	{
		// We also use PreviewSetAnimPosition in PIE when not playing, as we can preview in PIE.
		bool bIsNotInPIEOrNotPlaying = (RuntimeObject.GetWorld() && !RuntimeObject.GetWorld()->HasBegunPlay()) || Player.GetPlaybackStatus() != EMovieScenePlayerStatus::Playing;
		return GIsEditor && bIsNotInPIEOrNotPlaying;
	}

	static bool CanPlayAnimation(USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* AnimAssetBase)
	{
		return (SkeletalMeshComponent->GetSkeletalMeshAsset() &&
			SkeletalMeshComponent->GetSkeletalMeshAsset()->GetSkeleton() &&
			AnimAssetBase != nullptr &&
			AnimAssetBase->GetSkeleton() != nullptr);
	}

	static UAnimInstance* GetSourceAnimInstance(USkeletalMeshComponent* SkeletalMeshComponent)
	{
		UAnimInstance* SkelAnimInstance = SkeletalMeshComponent->GetAnimInstance();
		ISequencerAnimationSupport* SeqInterface = Cast<ISequencerAnimationSupport>(SkelAnimInstance);
		if (SeqInterface)
		{
			return SeqInterface->GetSourceAnimInstance();
		}

		return SkelAnimInstance;
	}

	static USkeletalMeshComponent* SkeletalMeshComponentFromObject(UObject* InObject)
	{
		// Check if we are bound directly to a skeletal mesh component.
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObject);
		if (SkeletalMeshComponent)
		{
			return SkeletalMeshComponent;
		}

		// Then check to see if we are controlling an actor. If so use its first skeletal mesh component.
		AActor* Actor = Cast<AActor>(InObject);
		if (!Actor)
		{
			if (UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(InObject))
			{
				Actor = ChildActorComponent->GetChildActor();
			}
		}
		if (Actor)
		{
			return Actor->FindComponentByClass<USkeletalMeshComponent>();
		}
		return nullptr;
	}

private:

	/** Parameter structure for setting the skeletal animation position */
	struct FSetAnimPositionParams
	{
		FMovieSceneEntityID EntityID;
		FRootInstanceHandle RootInstanceHandle;

		const UMovieSceneSkeletalAnimationSection* Section = nullptr;
		USkeletalMeshComponent* SkeletalMeshComponent = nullptr;

		FFrameTime CurrentTime;
		float FromPosition;
		float ToPosition;
		float Weight;

		bool bWantsRestoreState;
		bool bPlaying;
		bool bFireNotifies;
		bool bResetDynamics;
	};

	void SimulateMotionVectors(USkeletalMeshComponent* SkeletalMeshComponent, UMovieSceneMotionVectorSimulationSystem* MotionVectorSim)
	{
		for (USceneComponent* Child : SkeletalMeshComponent->GetAttachChildren())
		{
			if (!Child)
			{
				continue;
			}

			FName SocketName = Child->GetAttachSocketName();
			if (SocketName != NAME_None)
			{
				FTransform SocketTransform = SkeletalMeshComponent->GetSocketTransform(SocketName, RTS_Component);
				MotionVectorSim->AddSimulatedTransform(SkeletalMeshComponent, SocketTransform, SocketName);
			}
		}
	}

	void ApplyAnimations(
		IMovieScenePlayer& Player,
		USkeletalMeshComponent* SkeletalMeshComponent,
		TArrayView<const FActiveSkeletalAnimation> SkeletalAnimations,
		bool bPreviewPlayback,
		bool bFireNotifies,
		bool bResetDynamics)
	{
		const EMovieScenePlayerStatus::Type PlayerStatus = Player.GetPlaybackStatus();

		for (const FActiveSkeletalAnimation& SkeletalAnimation : SkeletalAnimations)
		{
			const UMovieSceneSkeletalAnimationSection* AnimSection = SkeletalAnimation.AnimSection;
			const FMovieSceneSkeletalAnimationParams& AnimParams = AnimSection->Params;

			// Don't fire notifies if looping around.
			bool bLooped = false;
			if (AnimParams.bReverse)
			{
				if (SkeletalAnimation.FromEvalTime <= SkeletalAnimation.ToEvalTime)
				{
					bLooped = true;
				}
			}
			else if (SkeletalAnimation.FromEvalTime >= SkeletalAnimation.ToEvalTime)
			{
				bLooped = true;
			}

			FSetAnimPositionParams SetAnimPositionParams;
			SetAnimPositionParams.EntityID = SkeletalAnimation.EntityID;
			SetAnimPositionParams.RootInstanceHandle = SkeletalAnimation.RootInstanceHandle;
			SetAnimPositionParams.Section = AnimSection;
			SetAnimPositionParams.SkeletalMeshComponent = SkeletalMeshComponent;
			SetAnimPositionParams.CurrentTime = SkeletalAnimation.Context.GetTime();
			SetAnimPositionParams.FromPosition = SkeletalAnimation.FromEvalTime;
			SetAnimPositionParams.ToPosition = SkeletalAnimation.ToEvalTime;
			SetAnimPositionParams.Weight = SkeletalAnimation.BlendWeight;
			SetAnimPositionParams.bWantsRestoreState = SkeletalAnimation.bWantsRestoreState;
			SetAnimPositionParams.bPlaying = (PlayerStatus == EMovieScenePlayerStatus::Playing);
			SetAnimPositionParams.bFireNotifies = (bFireNotifies && !AnimParams.bSkipAnimNotifiers && !bLooped);
			SetAnimPositionParams.bResetDynamics = bResetDynamics;

			if (bPreviewPlayback)
			{
				PreviewSetAnimPosition(SetAnimPositionParams);
			}
			else
			{
				SetAnimPosition(SetAnimPositionParams);
			}
		}
	}
	
	// Determines whether the bound object has a component transform property tag
	bool ContainsTransform(UObject* InBoundObject) const
	{
		using namespace UE::MovieScene;

		bool bContainsTransform = false;

		auto HarvestTransforms = [InBoundObject, &bContainsTransform](UObject* BoundObject)
		{
			if (BoundObject == InBoundObject)
			{
				bContainsTransform = true;
			}
		};
				
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		FMovieSceneTracksComponentTypes* Components = FMovieSceneTracksComponentTypes::Get();
		FEntityTaskBuilder()
			.Read(BuiltInComponents->BoundObject)
			// Only include component transforms
			.FilterAll({ Components->ComponentTransform.PropertyTag })
			// Only read things with the resolved properties on - this ensures we do not read any intermediate component transforms for blended properties
			.FilterAny({ BuiltInComponents->CustomPropertyIndex, BuiltInComponents->FastPropertyOffset, BuiltInComponents->SlowProperty })
			.Iterate_PerEntity(&Linker->EntityManager, HarvestTransforms);

		return bContainsTransform;
	}

	// Get the current transform for the component that the root bone will be swaped to
	TOptional<FTransform> GetCurrentTransform(ESwapRootBone SwapRootBone, USkeletalMeshComponent* SkeletalMeshComponent) const
	{
		TOptional<FTransform> CurrentTransform;
		if (SwapRootBone == ESwapRootBone::SwapRootBone_Component)
		{
			if (ContainsTransform(SkeletalMeshComponent))
			{
				CurrentTransform = SkeletalMeshComponent->GetRelativeTransform();
			}
		}
		else if (SwapRootBone == ESwapRootBone::SwapRootBone_Actor)
		{
			if (AActor* Actor = SkeletalMeshComponent->GetOwner())
			{
				if (USceneComponent* RootComponent = Actor->GetRootComponent())
				{
					if (ContainsTransform(RootComponent))
					{
						CurrentTransform = RootComponent->GetRelativeTransform();
					}
				}
			}
		}

		return CurrentTransform;
	}

	void SetAnimPosition(const FSetAnimPositionParams& Params)
	{
		static const bool bLooping = false;

		const FMovieSceneSkeletalAnimationParams& AnimParams = Params.Section->Params;
		if (!CanPlayAnimation(Params.SkeletalMeshComponent, AnimParams.Animation))
		{
			return;
		}
		if (AnimParams.bForceCustomMode)
		{
			Params.SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustomMode);
		}

		UAnimSequencerInstance* SequencerInst = GetAnimSequencerInstance(Params.SkeletalMeshComponent);
		if (SequencerInst)
		{
			PreAnimatedAnimInstanceStorage->BeginTrackingEntity(Params.EntityID, Params.bWantsRestoreState, Params.RootInstanceHandle, SequencerInst);
			PreAnimatedAnimInstanceStorage->CachePreAnimatedValue(FCachePreAnimatedValueParams(), SequencerInst);
			
			TOptional <FRootMotionOverride> RootMotion;
			UMovieSceneSkeletalAnimationSection::FRootMotionParams RootMotionParams;

			Params.Section->GetRootMotion(Params.CurrentTime, RootMotionParams);
			if (RootMotionParams.Transform.IsSet())
			{
				RootMotion = FRootMotionOverride();
				RootMotion.GetValue().RootMotion = RootMotionParams.Transform.GetValue();
				RootMotion.GetValue().bBlendFirstChildOfRoot = RootMotionParams.bBlendFirstChildOfRoot;
				RootMotion.GetValue().ChildBoneIndex = RootMotionParams.ChildBoneIndex;
				RootMotion.GetValue().PreviousTransform = RootMotionParams.PreviousTransform.GetValue();
			}

			// Use the section's address as the ID for the anim sequence.
			const uint32 AnimSequenceID = GetTypeHash(Params.Section);

			// If Sequencer has a transform track, we want to set the initial transform so that root motion (if it exists) can be applied relative to that.
			TOptional<FTransform> CurrentTransform = GetCurrentTransform(Params.Section->Params.SwapRootBone, Params.SkeletalMeshComponent);

			FAnimSequencerData AnimSequencerData(
					AnimParams.Animation,
					AnimSequenceID,
					RootMotion, 
					Params.FromPosition,
					Params.ToPosition,
					Params.Weight, 
					Params.bFireNotifies, 
					Params.Section->Params.SwapRootBone, 
					CurrentTransform,
					Params.Section->Params.MirrorDataTable.Get());
			SequencerInst->UpdateAnimTrackWithRootMotion(AnimSequencerData);
		}
		else if (UAnimInstance* AnimInst = GetSourceAnimInstance(Params.SkeletalMeshComponent))
		{
			FMontagePlayerPerSectionData* SectionData = SystemData.MontageData.Find(Params.Section);

			int32 InstanceId = (SectionData) ? SectionData->MontageInstanceId : INDEX_NONE;

			const float AssetPlayRate = FMath::IsNearlyZero(AnimParams.Animation->RateScale) ? 1.0f : AnimParams.Animation->RateScale;
			TWeakObjectPtr<UAnimMontage> WeakMontage = FAnimMontageInstance::SetSequencerMontagePosition(
					AnimParams.SlotName, 
					AnimInst, 
					InstanceId, 
					AnimParams.Animation, 
					Params.FromPosition / AssetPlayRate, 
					Params.ToPosition / AssetPlayRate, 
					Params.Weight, 
					bLooping, 
					Params.bPlaying);

			UAnimMontage* Montage = WeakMontage.Get();
			if (Montage)
			{
				FMontagePlayerPerSectionData& DataContainer = SystemData.MontageData.FindOrAdd(Params.Section);
				DataContainer.Montage = WeakMontage;
				DataContainer.MontageInstanceId = InstanceId;

				PreAnimatedMontageStorage->BeginTrackingEntity(Params.EntityID, Params.bWantsRestoreState, Params.RootInstanceHandle, Montage);
				PreAnimatedMontageStorage->CachePreAnimatedValue(FCachePreAnimatedValueParams(), Montage, [=](const FObjectKey& Unused) {
						FPreAnimatedSkeletalAnimationMontageState OutState;
						OutState.WeakInstance = AnimInst;
						OutState.MontageInstanceId = InstanceId;
						return OutState;
					});

				// Make sure it's playing if the sequence is.
				FAnimMontageInstance* Instance = AnimInst->GetMontageInstanceForID(InstanceId);
				Instance->bPlaying = Params.bPlaying;
			}
		}
	}

	void PreviewSetAnimPosition(const FSetAnimPositionParams& Params)
	{
		using namespace UE::MovieScene;

		static const bool bLooping = false;

		const FMovieSceneSkeletalAnimationParams& AnimParams = Params.Section->Params;
		if (!CanPlayAnimation(Params.SkeletalMeshComponent, AnimParams.Animation))
		{
			return;
		}
		if (AnimParams.bForceCustomMode)
		{
			Params.SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustomMode);
		}
		UAnimSequencerInstance* SequencerInst = GetAnimSequencerInstance(Params.SkeletalMeshComponent);
		if (SequencerInst)
		{
			PreAnimatedAnimInstanceStorage->BeginTrackingEntity(Params.EntityID, Params.bWantsRestoreState, Params.RootInstanceHandle, SequencerInst);
			PreAnimatedAnimInstanceStorage->CachePreAnimatedValue(FCachePreAnimatedValueParams(), SequencerInst);

			TOptional <FRootMotionOverride> RootMotion;
			UMovieSceneSkeletalAnimationSection::FRootMotionParams RootMotionParams;
			Params.Section->GetRootMotion(Params.CurrentTime, RootMotionParams);
			if (RootMotionParams.Transform.IsSet())
			{
				RootMotion = FRootMotionOverride();
				RootMotion.GetValue().RootMotion = RootMotionParams.Transform.GetValue();
				RootMotion.GetValue().ChildBoneIndex = RootMotionParams.ChildBoneIndex;
				RootMotion.GetValue().bBlendFirstChildOfRoot = RootMotionParams.bBlendFirstChildOfRoot;
				RootMotion.GetValue().PreviousTransform = RootMotionParams.PreviousTransform.GetValue();
			}

			// Use the section's address as the ID for the anim sequence.
			const uint32 AnimSequenceID = GetTypeHash(Params.Section);

			// If Sequencer has a transform track, we want to set the initial transform so that root motion (if it exists) can be applied relative to that.
			TOptional<FTransform> CurrentTransform = GetCurrentTransform(Params.Section->Params.SwapRootBone, Params.SkeletalMeshComponent);

			FAnimSequencerData AnimSequencerData(
					AnimParams.Animation,
					AnimSequenceID,
					RootMotion,
					Params.FromPosition,
					Params.ToPosition,
					Params.Weight,
					Params.bFireNotifies,
					Params.Section->Params.SwapRootBone,
					CurrentTransform,
					Params.Section->Params.MirrorDataTable.Get());
			SequencerInst->UpdateAnimTrackWithRootMotion(AnimSequencerData);
		}
		else if (UAnimInstance* AnimInst = GetSourceAnimInstance(Params.SkeletalMeshComponent))
		{
			FMontagePlayerPerSectionData* SectionData = SystemData.MontageData.Find(Params.Section);

			int32 InstanceId = SectionData ? SectionData->MontageInstanceId : INDEX_NONE;

			const float AssetPlayRate = FMath::IsNearlyZero(AnimParams.Animation->RateScale) ? 1.0f : AnimParams.Animation->RateScale;
			TWeakObjectPtr<UAnimMontage> WeakMontage = FAnimMontageInstance::PreviewSequencerMontagePosition(
					AnimParams.SlotName,
					Params.SkeletalMeshComponent,
					AnimInst,
					InstanceId,
					AnimParams.Animation,
					Params.FromPosition / AssetPlayRate,
					Params.ToPosition / AssetPlayRate,
					Params.Weight,
					bLooping,
					Params.bFireNotifies,
					Params.bPlaying);

			UAnimMontage* Montage = WeakMontage.Get();
			if (Montage)
			{
				FMontagePlayerPerSectionData& DataContainer = SystemData.MontageData.FindOrAdd(Params.Section);
				DataContainer.Montage = WeakMontage;
				DataContainer.MontageInstanceId = InstanceId;

				PreAnimatedMontageStorage->BeginTrackingEntity(Params.EntityID, Params.bWantsRestoreState, Params.RootInstanceHandle, Montage);
				PreAnimatedMontageStorage->CachePreAnimatedValue(FCachePreAnimatedValueParams(), Montage, [=](const FObjectKey& Unused) {
						FPreAnimatedSkeletalAnimationMontageState OutState;
						OutState.WeakInstance = AnimInst;
						OutState.MontageInstanceId = InstanceId;
						return OutState;
					});

				FAnimMontageInstance* Instance = AnimInst->GetMontageInstanceForID(InstanceId);
				Instance->bPlaying = Params.bPlaying;
			}

			if (Params.bResetDynamics)
			{
				// Make sure we reset any simulations.
				AnimInst->ResetDynamics(ETeleportType::ResetPhysics);
			}
		}
	}
};

} // namespace UE::MovieScene

UMovieSceneSkeletalAnimationSystem::UMovieSceneSkeletalAnimationSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();
	RelevantComponent = TrackComponents->SkeletalAnimation;
	Phase = ESystemPhase::Instantiation | ESystemPhase::Evaluation;

	SystemCategories |= FSystemInterrogator::GetExcludedFromInterrogationCategory();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UWeightAndEasingEvaluatorSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UMovieSceneComponentTransformSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UMovieSceneQuaternionInterpolationRotationSystem::StaticClass(), GetClass());
	}
}

void UMovieSceneSkeletalAnimationSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	const TStatId GatherStatId = GET_STATID(MovieSceneEval_GatherSkeletalAnimations);

	const FMovieSceneEntitySystemRunner* Runner = Linker->GetActiveRunner();
	if (Runner->GetCurrentPhase() == ESystemPhase::Instantiation)
	{
		CleanSystemData();
		return;
	}

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	// Start fresh every frame, gathering all active skeletal animations.
	SystemData.SkeletalAnimations.Reset();

	FGraphEventRef GatherTask = FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->RootInstanceHandle)
	.Read(BuiltInComponents->InstanceHandle)
	.Read(BuiltInComponents->BoundObject)
	.Read(TrackComponents->SkeletalAnimation)
	.ReadOptional(BuiltInComponents->WeightAndEasingResult)
	.FilterNone({ BuiltInComponents->Tags.Ignored })
	.SetStat(GatherStatId)
	.Dispatch_PerAllocation<FGatherSkeletalAnimations>(&Linker->EntityManager, InPrerequisites, nullptr, 
			Linker->GetInstanceRegistry(), SystemData);

	FSystemTaskPrerequisites EvalPrereqs;
	if (GatherTask)
	{
		EvalPrereqs.AddRootTask(GatherTask);
	}

	// Now evaluate gathered animations. We need to do this on the game thread (when in multi-threaded mode)
	// because this task will call into a lot of animation system code that needs to be called there.
	FEntityTaskBuilder()
	.SetStat(GET_STATID(MovieSceneEval_EvaluateSkeletalAnimations))
	.SetDesiredThread(Linker->EntityManager.GetGatherThread())
	.Dispatch<FEvaluateSkeletalAnimations>(&Linker->EntityManager, EvalPrereqs, &Subsequents, Linker, SystemData);
}

void UMovieSceneSkeletalAnimationSystem::CleanSystemData()
{
	// Clean-up old montage data.
	for (auto It = SystemData.MontageData.CreateIterator(); It; ++It)
	{
		if (It.Key().ResolveObjectPtr() == nullptr)
		{
			It.RemoveCurrent();
		}
	}
}

