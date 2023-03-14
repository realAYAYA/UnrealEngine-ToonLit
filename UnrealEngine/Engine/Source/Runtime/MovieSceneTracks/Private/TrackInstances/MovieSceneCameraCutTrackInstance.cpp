// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackInstances/MovieSceneCameraCutTrackInstance.h"
#include "ContentStreaming.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "GameFramework/Actor.h"
#include "Generators/MovieSceneEasingCurves.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneTimeHelpers.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneCameraCutTrackInstance)

DECLARE_CYCLE_STAT(TEXT("Camera Cut Track Token Execute"), MovieSceneEval_CameraCutTrack_TokenExecute, STATGROUP_MovieSceneEval);

namespace UE
{
namespace MovieScene
{

	/** Information about a camera cut's easing (in or out) */
	struct FBlendedCameraCutEasingInfo
	{
		float RootBlendTime = -1.f;
		TOptional<EMovieSceneBuiltInEasing> BlendType;

		FBlendedCameraCutEasingInfo() {}
		FBlendedCameraCutEasingInfo(float InRootBlendTime, const TScriptInterface<IMovieSceneEasingFunction>& EasingFunction)
		{
			RootBlendTime = InRootBlendTime;

			// If it's a built-in easing function, get the curve type. We'll try to convert it to what the
			// player controller knows later, in the movie scene player.
			const UObject* EaseInScript = EasingFunction.GetObject();
			if (const UMovieSceneBuiltInEasingFunction* BuiltInEaseIn = Cast<UMovieSceneBuiltInEasingFunction>(EaseInScript))
			{
				BlendType = BuiltInEaseIn->Type;
			}
		}
	};

	/** Camera cut info struct. */
	struct FBlendedCameraCut
	{
		FInstanceHandle InstanceHandle;
		TObjectPtr<UMovieSceneSection> Section;

		FMovieSceneObjectBindingID CameraBindingID;
		FMovieSceneSequenceID OperandSequenceID;

		FFrameNumber LocalStartTime;
		FFrameNumber LocalEaseInEndTime;
		FFrameTime LocalContextTime;
		FFrameNumber LocalEaseOutStartTime;
		FFrameNumber LocalEndTime;

		FBlendedCameraCutEasingInfo EaseIn;
		FBlendedCameraCutEasingInfo EaseOut;
		bool bLockPreviousCamera = false;

		FMovieSceneObjectBindingID PreviousCameraBindingID;
		FMovieSceneSequenceID PreviousOperandSequenceID;

		float PreviewBlendFactor = -1.f;
		bool bCanBlend = false;

		FBlendedCameraCut()
		{}
		FBlendedCameraCut(const FMovieSceneTrackInstanceInput& InInput, FMovieSceneObjectBindingID InCameraBindingID, FMovieSceneSequenceID InOperandSequenceID) 
			: InstanceHandle(InInput.InstanceHandle)
			, Section(InInput.Section)
			, CameraBindingID(InCameraBindingID)
			, OperandSequenceID(InOperandSequenceID)
		{}
	};

	/** Pre-roll camera cut info struct. */
	struct FPreRollCameraCut
	{
		FInstanceHandle InstanceHandle;
		FMovieSceneObjectBindingID CameraBindingID;
		FTransform CutTransform;
		bool bHasCutTransform;
	};

	/** A movie scene pre-animated token that stores a pre-animated camera cut */
	struct FCameraCutPreAnimatedToken : IMovieScenePreAnimatedGlobalToken
	{
		static FMovieSceneAnimTypeID GetAnimTypeID()
		{
			return TMovieSceneAnimTypeID<FCameraCutPreAnimatedToken>();
		}

		virtual void RestoreState(const FRestoreStateParams& RestoreParams) override
		{
			IMovieScenePlayer* Player = RestoreParams.GetTerminalPlayer();
			if (!ensure(Player))
			{
				return;
			}
			
			EMovieSceneCameraCutParams Params;
			Player->UpdateCameraCut(nullptr, Params);
		}
	};

	/** The producer class for the pre-animated token above */
	struct FCameraCutPreAnimatedTokenProducer : IMovieScenePreAnimatedGlobalTokenProducer
	{
		virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
		{
			return FCameraCutPreAnimatedToken();
		}
	};

	struct FCameraCutAnimator
	{
		static UObject* FindBoundObject(FMovieSceneObjectBindingID BindingID, FMovieSceneSequenceID SequenceID, IMovieScenePlayer& Player)
		{
			TArrayView<TWeakObjectPtr<>> Objects = BindingID.ResolveBoundObjects(SequenceID, Player);
			if (Objects.Num() > 0)
			{
				return Objects[0].Get();
			}
			return nullptr;
		}

		static bool MatchesCameraCutCache(UObject* CameraActor, const FBlendedCameraCut& Params, const UMovieSceneCameraCutTrackInstance::FCameraCutCache& CameraCutCache)
		{
			return CameraActor == CameraCutCache.LastLockedCamera.Get() &&
				Params.InstanceHandle == CameraCutCache.LastInstanceHandle &&
				Params.Section == CameraCutCache.LastSection;
		}

		static void UpdateCameraCutCache(UObject* CameraActor, const FBlendedCameraCut& Params, UMovieSceneCameraCutTrackInstance::FCameraCutCache& OutCameraCutCache)
		{
			OutCameraCutCache.LastLockedCamera = CameraActor;
			OutCameraCutCache.LastInstanceHandle = Params.InstanceHandle;
			OutCameraCutCache.LastSection = Params.Section;
		}

		static void AnimatePreRoll(const FPreRollCameraCut& Params, const FMovieSceneContext& Context, const FMovieSceneSequenceID& SequenceID, IMovieScenePlayer& Player)
		{
			if (Params.bHasCutTransform)
			{
				FVector Location = Params.CutTransform.GetLocation();
				IStreamingManager::Get().AddViewLocation(Location);
			}
			else
			{
				UObject* CameraObject = FindBoundObject(Params.CameraBindingID, SequenceID, Player);

				if (AActor* Actor = Cast<AActor>(CameraObject))
				{
					FVector Location = Actor->GetActorLocation();
					IStreamingManager::Get().AddViewLocation(Location);
				}
			}
		}

		static bool AnimateBlendedCameraCut(const FBlendedCameraCut& Params, UMovieSceneCameraCutTrackInstance::FCameraCutCache& CameraCutCache, const FMovieSceneContext& Context, IMovieScenePlayer& Player)
		{
			UObject* CameraActor = FindBoundObject(Params.CameraBindingID, Params.OperandSequenceID, Player);

			EMovieSceneCameraCutParams CameraCutParams;
			CameraCutParams.bJumpCut = Context.HasJumped();
			CameraCutParams.BlendTime = Params.EaseIn.RootBlendTime;
			CameraCutParams.BlendType = Params.EaseIn.BlendType;
			CameraCutParams.bLockPreviousCamera = Params.bLockPreviousCamera;

#if WITH_EDITOR
			UObject* PreviousCameraActor = FindBoundObject(Params.PreviousCameraBindingID, Params.PreviousOperandSequenceID, Player);
			CameraCutParams.PreviousCameraObject = PreviousCameraActor;
			CameraCutParams.PreviewBlendFactor = Params.PreviewBlendFactor;
			CameraCutParams.bCanBlend = Params.bCanBlend;
#endif

			static const FMovieSceneAnimTypeID CameraAnimTypeID = FMovieSceneAnimTypeID::Unique();

			const bool bMatchesCache = MatchesCameraCutCache(CameraActor, Params, CameraCutCache);
			if (!bMatchesCache)
			{
				Player.SavePreAnimatedState(CameraAnimTypeID, FCameraCutPreAnimatedTokenProducer());

				CameraCutParams.UnlockIfCameraObject = CameraCutCache.LastLockedCamera.Get();
				Player.UpdateCameraCut(CameraActor, CameraCutParams);
				UpdateCameraCutCache(CameraActor, Params, CameraCutCache);
				return true;
			}
			else if (CameraActor || CameraCutParams.BlendTime > 0.f)
			{
				Player.SavePreAnimatedState(CameraAnimTypeID, FCameraCutPreAnimatedTokenProducer());
	
				Player.UpdateCameraCut(CameraActor, CameraCutParams);
				return true;
			}

			return false;
		}
	};

}  // namespace MovieScene
}  // namespace UE


void UMovieSceneCameraCutTrackInstance::OnAnimate()
{
	using namespace UE::MovieScene;

	// Gather active camera cuts, and triage pre-rolls from actual cuts.
	TArray<FPreRollCameraCut> CameraCutPreRolls;
	TArray<FBlendedCameraCut> CameraCutParams;
	const FInstanceRegistry* InstanceRegistry = GetLinker()->GetInstanceRegistry();

	for (const FCameraCutInputInfo& InputInfo : SortedInputInfos)
	{
		const FMovieSceneTrackInstanceInput& Input = InputInfo.Input;
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(Input.InstanceHandle);
		const FMovieSceneContext& Context = SequenceInstance.GetContext();
		IMovieScenePlayer* Player = SequenceInstance.GetPlayer();

		const UMovieSceneCameraCutSection* Section = Cast<const UMovieSceneCameraCutSection>(Input.Section);
		const FMovieSceneObjectBindingID CameraBindingID = Section->GetCameraBindingID();

		FTransform CutTransform = Section->InitialCameraCutTransform;
		const bool bHasCutTransform = Section->bHasInitialCameraCutTransform;

		const int32 PreRollFrames = Section->GetPreRollFrames();
		bool bIsSectionPreRoll = false;
		if (PreRollFrames > 0 && Section->HasStartFrame())
		{
			const FFrameNumber SectionStartTime = Section->GetTrueRange().GetLowerBoundValue();
			const TRange<FFrameNumber> PreRollRange(SectionStartTime - PreRollFrames, SectionStartTime);
			bIsSectionPreRoll = PreRollRange.Contains(Context.GetTime().FloorToFrame());
		}

		if (Context.IsPreRoll() || bIsSectionPreRoll)
		{
			FPreRollCameraCut PreRollCameraCut { Input.InstanceHandle, CameraBindingID, CutTransform, bHasCutTransform };
			CameraCutPreRolls.Add(PreRollCameraCut);
		}
		else
		{
			const UMovieSceneCameraCutTrack* Track = Section->GetTypedOuter<UMovieSceneCameraCutTrack>();
			const FMovieSceneTimeTransform SequenceToRootTransform = Context.GetSequenceToRootTransform();

			FBlendedCameraCut Params(Input, CameraBindingID, SequenceInstance.GetSequenceID());
			Params.bCanBlend = Track->bCanBlend;

			// Get start/current/end time.
			Params.LocalContextTime = Context.GetTime();
			const TRange<FFrameNumber> SectionRange(Section->GetTrueRange());
			Params.LocalStartTime = SectionRange.HasLowerBound() ? SectionRange.GetLowerBoundValue() : FFrameNumber(-MAX_int32);
			Params.LocalEndTime = SectionRange.HasUpperBound() ? SectionRange.GetUpperBoundValue() : FFrameNumber(MAX_int32);

			// Get ease-in/out info.
			Params.LocalEaseInEndTime = Params.LocalStartTime;
			Params.LocalEaseOutStartTime = Params.LocalEndTime;
			if (Section->HasStartFrame() && Section->Easing.GetEaseInDuration() > 0)
			{
				Params.LocalEaseInEndTime = Params.LocalStartTime + Section->Easing.GetEaseInDuration();
				const float RootEaseInTime = SequenceToRootTransform.TimeScale * Context.GetFrameRate().AsSeconds(FFrameNumber(Section->Easing.GetEaseInDuration()));
				Params.EaseIn = FBlendedCameraCutEasingInfo(RootEaseInTime, Section->Easing.EaseIn);
			}
			if (Section->HasEndFrame() && Section->Easing.GetEaseOutDuration() > 0)
			{
				Params.LocalEaseOutStartTime = Params.LocalEndTime - Section->Easing.GetEaseOutDuration();
				const float RootEaseOutTime = SequenceToRootTransform.TimeScale * Context.GetFrameRate().AsSeconds(FFrameNumber(Section->Easing.GetEaseOutDuration()));
				Params.EaseOut = FBlendedCameraCutEasingInfo(RootEaseOutTime, Section->Easing.EaseOut);
			}

			// Remember locking option.
			Params.bLockPreviousCamera = Section->bLockPreviousCamera;

			// Get preview blending.
			const float Weight = Section->EvaluateEasing(Context.GetTime());
			Params.PreviewBlendFactor = Weight;

			CameraCutParams.Add(Params);
		}
	}

	// For now we only support one pre-roll.
	if (CameraCutPreRolls.Num() > 0)
	{
		FPreRollCameraCut& CameraCutPreRoll = CameraCutPreRolls.Last();
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(CameraCutPreRoll.InstanceHandle);
		const FMovieSceneContext& Context = SequenceInstance.GetContext();
		IMovieScenePlayer* Player = SequenceInstance.GetPlayer();
		const FMovieSceneSequenceID SequenceID = SequenceInstance.GetSequenceID();
		FCameraCutAnimator::AnimatePreRoll(CameraCutPreRoll, Context, SequenceID, *Player);
	}

	// For now we only support 2 active camera cuts at most (with blending between them).
	// Remember that our param structs are sorted by hierarchical bias and global start time, i.e. the front
	// of the array has "lower" camera cuts (ones in sub-most sequences) that started most recently, while the
	// rear of the array has the "higher" camera cuts (ones in the top-most sequences) that started earlier.
	FBlendedCameraCut FinalCameraCut;

	if (CameraCutParams.Num() >= 2)
	{
		// Blending 2 camera cuts: keep track of what the next and previous shots are supposed to be.
		//
		// We know that CameraCutParams[0] has more priority than CameraCutParams[1], whether it's 
		// because it started more recently, or because it's in a lower sub-sequence (higher hierarchical bias).
		// We want to blend away from this priority cut only if we are currently in its blend-out. We'll
		// determine that thanks to the various times we saved on the info struct.
		const FBlendedCameraCut& PriorityCameraCut = CameraCutParams[0];
		if (PriorityCameraCut.LocalContextTime < PriorityCameraCut.LocalEaseInEndTime)
		{
			// Blending in from the other cut.
			const FBlendedCameraCut& PrevCameraCut = CameraCutParams[1];
			const FBlendedCameraCut& NextCameraCut = CameraCutParams[0];

			FinalCameraCut = NextCameraCut;
			FinalCameraCut.PreviousCameraBindingID = PrevCameraCut.CameraBindingID;
			FinalCameraCut.PreviousOperandSequenceID = PrevCameraCut.OperandSequenceID;
		}
		else if (PriorityCameraCut.LocalContextTime > PriorityCameraCut.LocalEaseOutStartTime)
		{
			// Blending out to the other cut.
			const FBlendedCameraCut& PrevCameraCut = CameraCutParams[0];
			const FBlendedCameraCut& NextCameraCut = CameraCutParams[1];

			FinalCameraCut = NextCameraCut;
			FinalCameraCut.PreviousCameraBindingID = PrevCameraCut.CameraBindingID;
			FinalCameraCut.PreviousOperandSequenceID = PrevCameraCut.OperandSequenceID;

			// We use the priority cut's ease-out info, instead of the other cut's ease-in info. This is
			// because it's more reliable:
			// - If we're just blending from one cut to the next, the editor should have made the ease-in
			//   and ease-out durations and types the same, based on the overlap between the 2 sections.
			//   So we can use either one and be fine.
			// - However we could be in the case that we're blending out from the last cut of a child sequence
			//   and back up to a cut in a parent sequence. That parent cut may have been active for a long
			//   time (overriden temporarily by the child sequence's cut), and would therefore have no
			//   ease-in information. The child cut would however have ease-out information, which we use.
			FinalCameraCut.EaseIn = PrevCameraCut.EaseOut;
			FinalCameraCut.EaseOut = FBlendedCameraCutEasingInfo();
			FinalCameraCut.PreviewBlendFactor = (1.0f - PrevCameraCut.PreviewBlendFactor);
			FinalCameraCut.bLockPreviousCamera = PrevCameraCut.bLockPreviousCamera;

			// We need to force this flag because we could be blending out from a track that does support
			// blending to a track that does not (e.g. a parent sequence track).
			FinalCameraCut.bCanBlend = true;
		}
		else
		{
			// Fully active.
			FinalCameraCut = PriorityCameraCut;
		}
	}
	else if (CameraCutParams.Num() == 1)
	{
		// Only one camera cut active.
		FinalCameraCut = CameraCutParams[0];

		// It may be blending out back to gameplay however.
		if (FinalCameraCut.LocalContextTime > FinalCameraCut.LocalEaseOutStartTime)
		{
			FinalCameraCut.PreviewBlendFactor = 1.0f - FinalCameraCut.PreviewBlendFactor;
			FinalCameraCut.EaseIn = FinalCameraCut.EaseOut;
			FinalCameraCut.EaseOut = FBlendedCameraCutEasingInfo();
			FinalCameraCut.PreviousCameraBindingID = FinalCameraCut.CameraBindingID;
			FinalCameraCut.PreviousOperandSequenceID = FinalCameraCut.OperandSequenceID;
			FinalCameraCut.CameraBindingID = FMovieSceneObjectBindingID();
			FinalCameraCut.OperandSequenceID = FMovieSceneSequenceID();
		}
	}

	if (CameraCutParams.Num() > 0)
	{
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(FinalCameraCut.InstanceHandle);
		const FMovieSceneContext& Context = SequenceInstance.GetContext();
		IMovieScenePlayer* Player = SequenceInstance.GetPlayer();
		if (FCameraCutAnimator::AnimateBlendedCameraCut(FinalCameraCut, CameraCutCache, Context, *Player))
		{
			// Track whether this ever evaluated to take control. If so, we'll want to remove control in OnDestroyed.
			if (FCameraCutUseData* PlayerUseCount = PlayerUseCounts.Find(Player))
			{
				PlayerUseCount->bValid = true;
				// Remember whether we had blending support the last time we took control of the viewport. This is also
				// for OnDestroyed.
				PlayerUseCount->bCanBlend = FinalCameraCut.bCanBlend;
			}
		}
	}
}

void UMovieSceneCameraCutTrackInstance::OnInputAdded(const FMovieSceneTrackInstanceInput& InInput)
{
	using namespace UE::MovieScene;

	const FInstanceRegistry* InstanceRegistry = GetLinker()->GetInstanceRegistry();
	const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InInput.InstanceHandle);
	IMovieScenePlayer* Player = SequenceInstance.GetPlayer();

	int32& UseCount = PlayerUseCounts.FindOrAdd(Player, FCameraCutUseData()).UseCount;
	++UseCount;
}

void UMovieSceneCameraCutTrackInstance::OnInputRemoved(const FMovieSceneTrackInstanceInput& InInput)
{
	using namespace UE::MovieScene;

	const FInstanceRegistry* InstanceRegistry = GetLinker()->GetInstanceRegistry();
	const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InInput.InstanceHandle);
	IMovieScenePlayer* Player = SequenceInstance.GetPlayer();

	if (FCameraCutUseData* PlayerUseCount = PlayerUseCounts.Find(Player))
	{
		PlayerUseCount->UseCount--;
		if (PlayerUseCount->UseCount == 0)
		{
			PlayerUseCounts.Remove(Player);
		}
	}
}

void UMovieSceneCameraCutTrackInstance::OnEndUpdateInputs()
{
	using namespace UE::MovieScene;

	const FInstanceRegistry* InstanceRegistry = GetLinker()->GetInstanceRegistry();

	// Rebuild our sorted input infos.
	TMap<FMovieSceneTrackInstanceInput, float> GlobalStartTimePerSection;
	for (const FCameraCutInputInfo& InputInfo : SortedInputInfos)
	{
		GlobalStartTimePerSection.Add(InputInfo.Input, InputInfo.GlobalStartTime);
	}

	SortedInputInfos.Reset();

	for (const FMovieSceneTrackInstanceInput& Input : GetInputs())
	{
		FCameraCutInputInfo InputInfo;
		InputInfo.Input = Input;

		if (const float* GlobalStartTime = GlobalStartTimePerSection.Find(Input))
		{
			InputInfo.GlobalStartTime = *GlobalStartTime;
		}
		else
		{
			const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(Input.InstanceHandle);
			IMovieScenePlayer* Player = SequenceInstance.GetPlayer();

			if (UObject* PlaybackContext = Player->GetPlaybackContext())
			{
				if (UWorld* World = PlaybackContext->GetWorld())
				{
					const float WorldTime = World->GetTimeSeconds();
					InputInfo.GlobalStartTime = WorldTime;
				}
			}
		}

		SortedInputInfos.Add(InputInfo);
	}

	// Sort all active camera cuts by hierarchical bias first, and when they started in absolute game time second.
	// Later (higher starting time) cuts are sorted first, so we can prioritize the latest camera cut that started.
	Algo::Sort(SortedInputInfos,
			[InstanceRegistry](const FCameraCutInputInfo& A, const FCameraCutInputInfo& B) -> bool
			{
				const FSequenceInstance& SeqA = InstanceRegistry->GetInstance(A.Input.InstanceHandle);
				const FSequenceInstance& SeqB = InstanceRegistry->GetInstance(B.Input.InstanceHandle);
				const int32 HierarchicalBiasA = SeqA.GetContext().GetHierarchicalBias();
				const int32 HierarchicalBiasB = SeqB.GetContext().GetHierarchicalBias();
				if (HierarchicalBiasA > HierarchicalBiasB)
				{
					return true;
				}
				else if (HierarchicalBiasA < HierarchicalBiasB)
				{
					return false;
				}
				else
				{
					return A.GlobalStartTime > B.GlobalStartTime;
				}
			});
}

void UMovieSceneCameraCutTrackInstance::OnDestroyed()
{
	using namespace UE::MovieScene;

	const FInstanceRegistry* InstanceRegistry = GetLinker()->GetInstanceRegistry();

	bool bRestoreCamera = false;
	for (const FCameraCutInputInfo& InputInfo : SortedInputInfos)
	{
		const FMovieSceneTrackInstanceInput& Input = InputInfo.Input;
		const FMovieSceneContext& Context = InstanceRegistry->GetInstance(Input.InstanceHandle).GetContext();

		const UMovieSceneCameraCutSection* Section = Cast<const UMovieSceneCameraCutSection>(Input.Section);
		if (Context.IsPreRoll())
		{
			continue;
		}

		EMovieSceneCompletionMode CompletionMode = EMovieSceneCompletionMode::KeepState;

		if (Section->EvalOptions.CompletionMode == EMovieSceneCompletionMode::ProjectDefault)
		{
			if (const UMovieSceneSequence* OuterSequence = Section->GetTypedOuter<const UMovieSceneSequence>())
			{
				CompletionMode = OuterSequence->DefaultCompletionMode;
			}
		}
		else
		{
			CompletionMode = Section->EvalOptions.CompletionMode;
		}

		if (CompletionMode == EMovieSceneCompletionMode::RestoreState)
		{
			bRestoreCamera = true;
			break;
		}
	}

	if (bRestoreCamera)
	{
		// All sequencer players actually point to the same player controller and view target in a given world,
		// so we only need to restore the pre-animated state on one sequencer player, like, say, the first one
		// we still have in use. And we only do that when we have no more inputs active (if we still have some
		// inputs active, regardless of what sequencer player they belong to, they still have control of the
		// player controller's view target, so we don't want to mess that up).
		//
		// TODO-ludovic: when we have proper splitscreen support, this should be changed heavily.
		//
		for (const TPair<IMovieScenePlayer*, FCameraCutUseData>& PlayerUseCount : PlayerUseCounts)
		{
			// Restore only if we ever took control
			if (PlayerUseCount.Value.bValid)
			{
				EMovieSceneCameraCutParams Params;
#if WITH_EDITOR
				Params.bCanBlend = PlayerUseCount.Value.bCanBlend;
#endif
				if (PlayerUseCount.Key)
				{
					PlayerUseCount.Key->UpdateCameraCut(nullptr, Params);
				}
				break;  // Only do it on the first one.
			}
		}
	}

	PlayerUseCounts.Reset();
}

