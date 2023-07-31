// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePlayer.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "Misc/CoreDelegates.h"
#include "EngineGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Tickable.h"
#include "Engine/LevelScriptActor.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneSubSection.h"
#include "LevelSequenceSpawnRegister.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LocalPlayer.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Systems/MovieSceneMotionVectorSimulationSystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "LevelSequenceActor.h"
#include "Modules/ModuleManager.h"
#include "LevelUtils.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "LevelSequenceModule.h"
#include "Generators/MovieSceneEasingCurves.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelSequencePlayer)

/* ULevelSequencePlayer structors
 *****************************************************************************/

ULevelSequencePlayer::ULevelSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}


/* ULevelSequencePlayer interface
 *****************************************************************************/

ULevelSequencePlayer* ULevelSequencePlayer::CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* InLevelSequence, FMovieSceneSequencePlaybackSettings Settings, ALevelSequenceActor*& OutActor)
{
	if (InLevelSequence == nullptr)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr || World->bIsTearingDown)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bAllowDuringConstructionScript = true;

	// Defer construction for autoplay so that BeginPlay() is called
	SpawnParams.bDeferConstruction = true;

	ALevelSequenceActor* Actor = World->SpawnActor<ALevelSequenceActor>(SpawnParams);

	Actor->PlaybackSettings = Settings;
	Actor->SequencePlayer->SetPlaybackSettings(Settings);

	Actor->SetSequence(InLevelSequence);

	Actor->InitializePlayer();
	OutActor = Actor;

	FTransform DefaultTransform;
	Actor->FinishSpawning(DefaultTransform);

	return Actor->SequencePlayer;
}

/* ULevelSequencePlayer implementation
 *****************************************************************************/

void ULevelSequencePlayer::Initialize(ULevelSequence* InLevelSequence, ULevel* InLevel, const FLevelSequenceCameraSettings& InCameraSettings)
{
	// Never use the level to resolve bindings unless we're playing back within a streamed or instanced level
	StreamedLevelAssetPath = FTopLevelAssetPath();

	World = InLevel->OwningWorld;
	Level = InLevel;
	CameraSettings = InCameraSettings;

	// Construct the path to the level asset that the streamed level relates to
	ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel(InLevel);
	if (LevelStreaming)
	{
		// StreamedLevelPackage is a package name of the form /Game/Folder/MapName, not a full asset path
		FString StreamedLevelPackage = ((LevelStreaming->PackageNameToLoad == NAME_None) ? LevelStreaming->GetWorldAssetPackageFName() : LevelStreaming->PackageNameToLoad).ToString();

		int32 SlashPos = 0;
		if (StreamedLevelPackage.FindLastChar('/', SlashPos) && SlashPos < StreamedLevelPackage.Len()-1)
		{
			StreamedLevelAssetPath = FTopLevelAssetPath(*StreamedLevelPackage, &StreamedLevelPackage[SlashPos+1]);
		}
	}

	SpawnRegister = MakeShareable(new FLevelSequenceSpawnRegister);
	UMovieSceneSequencePlayer::Initialize(InLevelSequence);
}

void ULevelSequencePlayer::ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& InSequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	bool bAllowDefault = PlaybackClient ? PlaybackClient->RetrieveBindingOverrides(InBindingId, SequenceID, OutObjects) : true;

	if (bAllowDefault)
	{
		if (StreamedLevelAssetPath.IsValid() && ResolutionContext && ResolutionContext->IsA<UWorld>())
		{
			ResolutionContext = Level.Get();
		}

		if (ULevelSequence* LevelSequence = Cast<ULevelSequence>(&InSequence))
		{
			// Passing through the streamed level asset path ensures that bindings within instance sub levels resolve correctly
			LevelSequence->LocateBoundObjects(InBindingId, ResolutionContext, StreamedLevelAssetPath, OutObjects);
		}
		else
		{
			InSequence.LocateBoundObjects(InBindingId, ResolutionContext, OutObjects);
		}
	}
}

bool ULevelSequencePlayer::CanPlay() const
{
	return World.IsValid();
}

void ULevelSequencePlayer::OnStartedPlaying()
{
	EnableCinematicMode(true);
}

void ULevelSequencePlayer::OnStopped()
{
	EnableCinematicMode(false);

	if (World != nullptr && World->GetGameInstance() != nullptr)
	{
		APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();

		if (PC != nullptr)
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->bClientSimulatingViewTarget = false;
			}
		}
	}

	LastViewTarget.Reset();
}

void ULevelSequencePlayer::UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, const FMovieSceneUpdateArgs& Args)
{
	UMovieSceneSequencePlayer::UpdateMovieSceneInstance(InRange, PlayerStatus, Args);

	// TODO-ludovic: we should move this to a post-evaluation callback when the evaluation is asynchronous.
	FLevelSequencePlayerSnapshot NewSnapshot;
	TakeFrameSnapshot(NewSnapshot);

	if (!PreviousSnapshot.IsSet() || PreviousSnapshot.GetValue().CurrentShotName != NewSnapshot.CurrentShotName)
	{
		CSV_EVENT_GLOBAL(TEXT("%s"), *NewSnapshot.CurrentShotName);
		//UE_LOG(LogMovieScene, Log, TEXT("Shot evaluated: '%s'"), *NewSnapshot.CurrentShotName);
	}

	PreviousSnapshot = NewSnapshot;
}

/* IMovieScenePlayer interface
 *****************************************************************************/

TTuple<EViewTargetBlendFunction, float> BuiltInEasingTypeToBlendFunction(EMovieSceneBuiltInEasing EasingType)
{
	using Return = TTuple<EViewTargetBlendFunction, float>;
	switch (EasingType)
	{
		case EMovieSceneBuiltInEasing::Linear:
			return Return(EViewTargetBlendFunction::VTBlend_Linear, 1.f);

		case EMovieSceneBuiltInEasing::QuadIn:
			return Return(EViewTargetBlendFunction::VTBlend_EaseIn, 2);
		case EMovieSceneBuiltInEasing::QuadOut:
			return Return(EViewTargetBlendFunction::VTBlend_EaseOut, 2);
		case EMovieSceneBuiltInEasing::QuadInOut:
			return Return(EViewTargetBlendFunction::VTBlend_EaseInOut, 2);

		case EMovieSceneBuiltInEasing::CubicIn:
			return Return(EViewTargetBlendFunction::VTBlend_EaseIn, 3);
		case EMovieSceneBuiltInEasing::CubicOut:
			return Return(EViewTargetBlendFunction::VTBlend_EaseOut, 3);
		case EMovieSceneBuiltInEasing::CubicInOut:
			return Return(EViewTargetBlendFunction::VTBlend_EaseInOut, 3);

		case EMovieSceneBuiltInEasing::QuartIn:
			return Return(EViewTargetBlendFunction::VTBlend_EaseIn, 4);
		case EMovieSceneBuiltInEasing::QuartOut:
			return Return(EViewTargetBlendFunction::VTBlend_EaseOut, 4);
		case EMovieSceneBuiltInEasing::QuartInOut:
			return Return(EViewTargetBlendFunction::VTBlend_EaseInOut, 4);

		case EMovieSceneBuiltInEasing::QuintIn:
			return Return(EViewTargetBlendFunction::VTBlend_EaseIn, 5);
		case EMovieSceneBuiltInEasing::QuintOut:
			return Return(EViewTargetBlendFunction::VTBlend_EaseOut, 5);
		case EMovieSceneBuiltInEasing::QuintInOut:
			return Return(EViewTargetBlendFunction::VTBlend_EaseInOut, 5);

		// UNSUPPORTED
		case EMovieSceneBuiltInEasing::SinIn:
		case EMovieSceneBuiltInEasing::SinOut:
		case EMovieSceneBuiltInEasing::SinInOut:
		case EMovieSceneBuiltInEasing::CircIn:
		case EMovieSceneBuiltInEasing::CircOut:
		case EMovieSceneBuiltInEasing::CircInOut:
		case EMovieSceneBuiltInEasing::ExpoIn:
		case EMovieSceneBuiltInEasing::ExpoOut:
		case EMovieSceneBuiltInEasing::ExpoInOut:
			break;
	}
	return Return(EViewTargetBlendFunction::VTBlend_Linear, 1.f);
}

void ULevelSequencePlayer::UpdateCameraCut(UObject* CameraObject, const EMovieSceneCameraCutParams& CameraCutParams)
{
	UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);
	if (CameraComponent && CameraComponent->GetOwner() != CameraObject)
	{
		CameraObject = CameraComponent->GetOwner();
	}

	CachedCameraComponent = CameraComponent;
	
	if (World == nullptr || World->GetGameInstance() == nullptr)
	{
		return;
	}

	// skip missing player controller
	APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();

	if (PC == nullptr)
	{
		return;
	}

	// skip same view target
	AActor* ViewTarget = PC->GetViewTarget();

	if (!CanUpdateCameraCut())
	{
		return;
	}

	if (CameraObject == ViewTarget)
	{
		if (CameraCutParams.bJumpCut)
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->SetGameCameraCutThisFrame();
			}

			if (CameraComponent)
			{
				CameraComponent->NotifyCameraCut();
			}

			if (UMovieSceneMotionVectorSimulationSystem* MotionVectorSim = RootTemplateInstance.GetEntitySystemLinker()->FindSystem<UMovieSceneMotionVectorSimulationSystem>())
			{
				MotionVectorSim->SimulateAllTransforms();
			}
		}
		return;
	}

	// skip unlocking if the current view target differs
	AActor* UnlockIfCameraActor = Cast<AActor>(CameraCutParams.UnlockIfCameraObject);

	// if unlockIfCameraActor is valid, release lock if currently locked to object
	if (CameraObject == nullptr && UnlockIfCameraActor != nullptr && UnlockIfCameraActor != ViewTarget)
	{
		return;
	}

	// override the player controller's view target
	AActor* CameraActor = Cast<AActor>(CameraObject);
	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();

	// if the camera object is null, use the last view target so that it is restored to the state before the sequence takes control
	bool bRestoreAspectRatioConstraint = false;
	if (CameraActor == nullptr)
	{
		CameraActor = LastViewTarget.Get();
		bRestoreAspectRatioConstraint = true;

		// Skip if the last view target is the same as the current view target so that there's no additional camera cut
		if (CameraActor == ViewTarget)
		{
			if (LocalPlayer && LastAspectRatioAxisConstraint.IsSet())
			{
				LocalPlayer->AspectRatioAxisConstraint = LastAspectRatioAxisConstraint.GetValue();
			}
			return;
		}
	}

	// Save the last view target/aspect ratio constraint/etc. so that it can all be restored when the camera object is null.
	if (!LastViewTarget.IsValid())
	{
		LastViewTarget = ViewTarget;
	}
	if (!LastAspectRatioAxisConstraint.IsSet())
	{
		if (LocalPlayer != nullptr)
		{
			LastAspectRatioAxisConstraint = LocalPlayer->AspectRatioAxisConstraint;
		}
	}

	bool bDoSetViewTarget = true;
	FViewTargetTransitionParams TransitionParams;
	if (CameraCutParams.BlendType.IsSet())
	{
		UE_LOG(LogLevelSequence, Log, TEXT("Blending into new camera cut: '%s' -> '%s' (blend time: %f)"),
			(ViewTarget ? *ViewTarget->GetName() : TEXT("None")),
			(CameraObject ? *CameraObject->GetName() : TEXT("None")),
			TransitionParams.BlendTime);

		// Convert known easing functions to their corresponding view target blend parameters.
		TTuple<EViewTargetBlendFunction, float> BlendFunctionAndExp = BuiltInEasingTypeToBlendFunction(CameraCutParams.BlendType.GetValue());
		TransitionParams.BlendTime = CameraCutParams.BlendTime;
		TransitionParams.bLockOutgoing = CameraCutParams.bLockPreviousCamera;
		TransitionParams.BlendFunction = BlendFunctionAndExp.Get<0>();
		TransitionParams.BlendExp = BlendFunctionAndExp.Get<1>();

		// Calling SetViewTarget on a camera that we are currently transitioning to will 
		// result in that transition being aborted, and the view target being set immediately.
		// We want to avoid that, so let's leave the transition running if it's the case.
		if (PC->PlayerCameraManager != nullptr)
		{
			const AActor* CurViewTarget = PC->PlayerCameraManager->ViewTarget.Target;
			const AActor* PendingViewTarget = PC->PlayerCameraManager->PendingViewTarget.Target;
			if (CameraActor != nullptr && PendingViewTarget == CameraActor)
			{
				UE_LOG(LogLevelSequence, Log, TEXT("Camera transition aborted, we are already blending towards the intended camera"));
				bDoSetViewTarget = false;
			}
		}
	}
	else
	{
		UE_LOG(LogLevelSequence, Log, TEXT("Starting new camera cut: '%s'"),
			(CameraObject ? *CameraObject->GetName() : TEXT("None")));
	}
	if (bDoSetViewTarget)
	{
		PC->SetViewTarget(CameraActor, TransitionParams);
	}

	// Set or restore the aspect ratio constraint if we were overriding it for this sequence.
	if (LocalPlayer != nullptr && CameraSettings.bOverrideAspectRatioAxisConstraint)
	{
		if (bRestoreAspectRatioConstraint)
		{
			check(LastAspectRatioAxisConstraint.IsSet());
			if (LastAspectRatioAxisConstraint.IsSet())
			{
				LocalPlayer->AspectRatioAxisConstraint = LastAspectRatioAxisConstraint.GetValue();
			}
		}
		else
		{
			LocalPlayer->AspectRatioAxisConstraint = CameraSettings.AspectRatioAxisConstraint;
		}
	}

	// we want to notify of cuts on hard cuts and time jumps, but not on blend cuts
	const bool bIsStraightCut = !CameraCutParams.BlendType.IsSet() || CameraCutParams.bJumpCut;

	if (CameraComponent && bIsStraightCut)
	{
		CameraComponent->NotifyCameraCut();
	}

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->bClientSimulatingViewTarget = (CameraActor != nullptr);

		if (bIsStraightCut)
		{
			PC->PlayerCameraManager->SetGameCameraCutThisFrame();
		}
	}

	if (bIsStraightCut)
	{
		if (UMovieSceneMotionVectorSimulationSystem* MotionVectorSim = RootTemplateInstance.GetEntitySystemLinker()->FindSystem<UMovieSceneMotionVectorSimulationSystem>())
		{
			MotionVectorSim->SimulateAllTransforms();
		}

		if (OnCameraCut.IsBound())
		{
			OnCameraCut.Broadcast(CameraComponent);
		}
	}
}

UObject* ULevelSequencePlayer::GetPlaybackContext() const
{
	return World.Get();
}

TArray<UObject*> ULevelSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (World.IsValid())
	{
		GetEventContexts(*World, EventContexts);
	}

	return EventContexts;
}

void ULevelSequencePlayer::GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts)
{
	if (InWorld.GetLevelScriptActor())
	{
		OutContexts.Add(InWorld.GetLevelScriptActor());
	}

	for (ULevelStreaming* StreamingLevel : InWorld.GetStreamingLevels())
	{
		if (StreamingLevel && StreamingLevel->GetLevelScriptActor())
		{
			OutContexts.Add(StreamingLevel->GetLevelScriptActor());
		}
	}
}

void ULevelSequencePlayer::TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const
{
	if (!ensure(Sequence))
	{
		return;
	}

	// In Play Rate Resolution
	const FFrameTime StartTimeWithoutWarmupFrames = SnapshotOffsetTime.IsSet() ? StartTime + SnapshotOffsetTime.GetValue() : StartTime;
	const FFrameTime CurrentPlayTime = PlayPosition.GetCurrentPosition();
	// In Playback Resolution
	const FFrameTime CurrentSequenceTime		  = ConvertFrameTime(CurrentPlayTime, PlayPosition.GetInputRate(), PlayPosition.GetOutputRate());

	OutSnapshot.MasterTime = FQualifiedFrameTime(CurrentPlayTime, PlayPosition.GetInputRate());
	OutSnapshot.MasterName = Sequence->GetName();

	OutSnapshot.CurrentShotName = OutSnapshot.MasterName;
	OutSnapshot.CurrentShotLocalTime = FQualifiedFrameTime(CurrentPlayTime, PlayPosition.GetInputRate());
	OutSnapshot.CameraComponent = CachedCameraComponent.IsValid() ? CachedCameraComponent.Get() : nullptr;
	OutSnapshot.ShotID = MovieSceneSequenceID::Invalid;

	OutSnapshot.ActiveShot = Cast<ULevelSequence>(Sequence);

	UMovieScene* MovieScene = Sequence->GetMovieScene();

#if WITH_EDITORONLY_DATA
	OutSnapshot.SourceTimecode = MovieScene->GetEarliestTimecodeSource().Timecode.ToString();
#endif

	UMovieSceneCinematicShotTrack* ShotTrack = MovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (ShotTrack)
	{
		UMovieSceneCinematicShotSection* ActiveShot = nullptr;
		for (UMovieSceneSection* Section : ShotTrack->GetAllSections())
		{
			if (!ensure(Section))
			{
				continue;
			}

			// It's unfortunate that we have to copy the logic of UMovieSceneCinematicShotTrack::GetRowCompilerRules() to some degree here, but there's no better way atm
			bool bThisShotIsActive = Section->IsActive();

			TRange<FFrameNumber> SectionRange = Section->GetRange();
			bThisShotIsActive = bThisShotIsActive && SectionRange.Contains(CurrentSequenceTime.FrameNumber);

			if (bThisShotIsActive && ActiveShot)
			{
				if (Section->GetRowIndex() < ActiveShot->GetRowIndex())
				{
					bThisShotIsActive = true;
				}
				else if (Section->GetRowIndex() == ActiveShot->GetRowIndex())
				{
					// On the same row - latest start wins
					bThisShotIsActive = TRangeBound<FFrameNumber>::MaxLower(SectionRange.GetLowerBound(), ActiveShot->GetRange().GetLowerBound()) == SectionRange.GetLowerBound();
				}
				else
				{
					bThisShotIsActive = false;
				}
			}

			if (bThisShotIsActive)
			{
				ActiveShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}

		if (ActiveShot)
		{
			// Assume that shots with no sequence start at 0.
			FMovieSceneSequenceTransform OuterToInnerTransform = ActiveShot->OuterToInnerTransform();
			UMovieSceneSequence*         InnerSequence = ActiveShot->GetSequence();
			FFrameRate                   InnerTickResoloution = InnerSequence ? InnerSequence->GetMovieScene()->GetTickResolution() : PlayPosition.GetOutputRate();
			FFrameRate                   InnerFrameRate = InnerSequence ? InnerSequence->GetMovieScene()->GetDisplayRate() : PlayPosition.GetInputRate();
			FFrameTime                   InnerDisplayTime = ConvertFrameTime(CurrentSequenceTime * OuterToInnerTransform, InnerTickResoloution, InnerFrameRate);

			OutSnapshot.CurrentShotName = ActiveShot->GetShotDisplayName();
			OutSnapshot.CurrentShotLocalTime = FQualifiedFrameTime(InnerDisplayTime, InnerFrameRate);
			OutSnapshot.ShotID = ActiveShot->GetSequenceID();
			OutSnapshot.ActiveShot = Cast<ULevelSequence>(ActiveShot->GetSequence());

#if WITH_EDITORONLY_DATA
			FFrameNumber  InnerFrameNumber = InnerFrameRate.AsFrameNumber(InnerFrameRate.AsSeconds(InnerDisplayTime));
			FFrameNumber  InnerStartFrameNumber = ActiveShot->TimecodeSource.Timecode.ToFrameNumber(InnerFrameRate);
			FFrameNumber  InnerCurrentFrameNumber = InnerStartFrameNumber + InnerFrameNumber;
			FTimecode     InnerCurrentTimecode = ActiveShot->TimecodeSource.Timecode.FromFrameNumber(InnerCurrentFrameNumber, InnerFrameRate, false);

			OutSnapshot.SourceTimecode = InnerCurrentTimecode.ToString();
#else
			OutSnapshot.SourceTimecode = FTimecode().ToString();
#endif
		}
	}
}

void ULevelSequencePlayer::EnableCinematicMode(bool bEnable)
{
	// iterate through the controller list and set cinematic mode if necessary
	bool bNeedsCinematicMode = PlaybackSettings.bDisableMovementInput || PlaybackSettings.bDisableLookAtInput || PlaybackSettings.bHidePlayer || PlaybackSettings.bHideHud;

	if (bNeedsCinematicMode)
	{
		if (World.IsValid())
		{
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PC = Iterator->Get();
				if (PC && PC->IsLocalController())
				{
					PC->SetCinematicMode(bEnable, PlaybackSettings.bHidePlayer, PlaybackSettings.bHideHud, PlaybackSettings.bDisableMovementInput, PlaybackSettings.bDisableLookAtInput);
				}
			}
		}
	}
}

void ULevelSequencePlayer::RewindForReplay()
{
	// Stop the sequence when starting to seek through a replay. This restores our state to be unmodified
	// in case the replay is seeking to before playback. If we're in the middle of playback after rewinding,
	// the replay will feed the correct packets to synchronize our playback time and state.
	Stop();

	NetSyncProps.LastKnownPosition = FFrameTime(0);
	NetSyncProps.LastKnownStatus = EMovieScenePlayerStatus::Stopped;
	NetSyncProps.LastKnownNumLoops = 0;
}

