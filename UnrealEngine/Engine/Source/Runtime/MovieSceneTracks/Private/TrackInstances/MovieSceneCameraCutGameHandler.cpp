// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackInstances/MovieSceneCameraCutGameHandler.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Evaluation/CameraCutPlaybackCapability.h"
#include "Generators/MovieSceneEasingCurves.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneCommonHelpers.h"
#include "Systems/MovieSceneMotionVectorSimulationSystem.h"
#include "TrackInstances/MovieSceneCameraCutTrackInstance.h"

namespace UE::MovieScene
{

bool FPreAnimatedCameraCutTraits::ShouldHandleWorldCameraCuts(UWorld* World)
{
	return World && 
		World->GetGameInstance() != nullptr &&
		World->WorldType != EWorldType::Editor &&
		World->WorldType != EWorldType::EditorPreview;
}

FPreAnimatedCameraCutState FPreAnimatedCameraCutTraits::CachePreAnimatedValue(
		IMovieScenePlayer* Player, 
		uint8 InKey)
{
	UObject* PlaybackContext = Player->GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
	if (ShouldHandleWorldCameraCuts(World))
	{
		CA_SUPPRESS(6011);
		APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();

		// Save previous view target.
		APlayerCameraManager* CameraManager = (PC != nullptr) ? PC->PlayerCameraManager : nullptr;
		AActor* ViewTarget = CameraManager ? CameraManager->GetViewTarget() : nullptr;

		// Save previous aspect ratio axis constraint.
		ULocalPlayer* LocalPlayer = (PC != nullptr) ? PC->GetLocalPlayer() : nullptr;
		TOptional<EAspectRatioAxisConstraint> AspectRatioAxisConstraint;
		if (LocalPlayer)
		{
			AspectRatioAxisConstraint = LocalPlayer->AspectRatioAxisConstraint;
		}

		return StorageType{ World, LocalPlayer, ViewTarget, AspectRatioAxisConstraint };
	}
	return StorageType();
}

void FPreAnimatedCameraCutTraits::RestorePreAnimatedValue(
		uint8 InKey, 
		const FPreAnimatedCameraCutState& CachedValue, 
		const FRestoreStateParams& Params)
{
	UWorld* World = Cast<UWorld>(CachedValue.LastWorld.ResolveObjectPtr());
	if (!ShouldHandleWorldCameraCuts(World))
	{
		return;
	}

	APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();
	APlayerCameraManager* CameraManager = (PC != nullptr) ? PC->PlayerCameraManager : nullptr;

	// Restore previous view target.
	// If the previous view target is not valid anymore, we still set it on the camera manger. This will by
	// default fall back to using the player controller as the view target.
	if (CameraManager)
	{
		AActor* PreviousViewTarget = Cast<AActor>(CachedValue.LastViewTarget.ResolveObjectPtr());
		CameraManager->SetViewTarget(PreviousViewTarget);
	}

	// Restore previous aspect ratio axis constraint. Use the cached local player if there's no local player
	// to be found, which can happen if pre-animated state is restored during level loads and such.
	ULocalPlayer* LocalPlayer = (PC != nullptr) ? 
		PC->GetLocalPlayer() : 
		Cast<ULocalPlayer>(CachedValue.LastLocalPlayer.ResolveObjectPtr());
	if (LocalPlayer && CachedValue.LastAspectRatioAxisConstraint.IsSet())
	{
		LocalPlayer->AspectRatioAxisConstraint = CachedValue.LastAspectRatioAxisConstraint.GetValue();
	}
}

TAutoRegisterPreAnimatedStorageID<FPreAnimatedCameraCutStorage> FPreAnimatedCameraCutStorage::StorageID;

/** Utility function for converting sequence blend curves to camera manager blend types */
static TTuple<EViewTargetBlendFunction, float> BuiltInEasingTypeToBlendFunction(EMovieSceneBuiltInEasing EasingType)
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

		case EMovieSceneBuiltInEasing::Cubic:
		case EMovieSceneBuiltInEasing::HermiteCubicInOut:
			return Return(EViewTargetBlendFunction::VTBlend_Cubic, 3);

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
		case EMovieSceneBuiltInEasing::Custom:
			break;
	}
	return Return(EViewTargetBlendFunction::VTBlend_Linear, 1.f);
}

void FCameraCutGameHandler::CachePreAnimatedValue(
		UMovieSceneEntitySystemLinker* Linker,
		const FSequenceInstance& SequenceInstance)
{
	TSharedPtr<FPreAnimatedCameraCutStorage> PreAnimatedStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedCameraCutStorage>();

	IMovieScenePlayer* Player = SequenceInstance.GetPlayer();
	PreAnimatedStorage->CachePreAnimatedValue(
			(uint8)0,  // Later this can be an index for split-screen player
			[Player](uint8 InKey) { return FPreAnimatedCameraCutTraits::CachePreAnimatedValue(Player, InKey); },
			EPreAnimatedCaptureSourceTracking::AlwaysCache);
}

FCameraCutGameHandler::FCameraCutGameHandler(
		UMovieSceneEntitySystemLinker* InLinker,
		const FSequenceInstance& InSequenceInstance)
	: Linker(InLinker)
	, SequenceInstance(InSequenceInstance)
{
}

void FCameraCutGameHandler::SetCameraCut(
		UObject* CameraObject,
		const FMovieSceneCameraCutParams& CameraCutParams)
{
	FCameraCutPlaybackCapabilityCompatibilityWrapper Wrapper(SequenceInstance);

	// If we don't want to update camera cuts, let's bail out.
	if (!Wrapper.ShouldUpdateCameraCut())
	{
		return;
	}

	IMovieScenePlayer* Player = SequenceInstance.GetPlayer();
	UObject* PlaybackContext = Player->GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;

	// Also bail out if we don't have a world running any sort of game.
	if (!FPreAnimatedCameraCutTraits::ShouldHandleWorldCameraCuts(World))
	{
		return;
	}

	CA_SUPPRESS(6011);
	APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();
	APlayerCameraManager* CameraManager = (PC != nullptr) ? PC->PlayerCameraManager : nullptr;

	// If the player controller is missing, there is no camera manager for us to manage the view target
	// so, again, we bail out.
	if (CameraManager == nullptr)
	{
		return;
	}

	// Let's get the current view target directly from the player camera manager first. This is because
	// we don't want to go through GetViewTarget, which checks if the current view target is valid, and
	// re-assigns it to the player controller if it's not. We don't want this to happen, especially since
	// a spawnable camera might have just been unspawned, causing SetCameraCut to be called, and we
	// need to handle this properly.
	AActor* ViewTarget = CameraManager->PendingViewTarget.Target;
	if (!ViewTarget)
	{
		ViewTarget = CameraManager->ViewTarget.Target;
	}

	// If UnlockIfCameraActor is valid, release lock only if currently locked to the specified object.
	AActor* UnlockIfCameraActor = Cast<AActor>(CameraCutParams.UnlockIfCameraObject);
	if (CameraObject == nullptr && UnlockIfCameraActor != nullptr && UnlockIfCameraActor != ViewTarget)
	{
		return;
	}

	// See if we need to override the aspect ratio axis constraint.
	TOptional<EAspectRatioAxisConstraint> OverrideAspectRatioAxisConstraint;
	if (Wrapper.CameraCutCapability)
	{
		OverrideAspectRatioAxisConstraint = Wrapper.CameraCutCapability->GetAspectRatioAxisConstraintOverride();
	}

	// CameraObject is null if we need to release control, which can happen here (instead of via pre-animated 
	// state restore) if we are *blending* back to gameplay, and not cutting back to it at the end of a camera 
	// cut section. Let's get the pre-animated value and blend back towards it.
	if (CameraObject == nullptr)
	{
		TSharedPtr<FPreAnimatedCameraCutStorage> PreAnimatedStorage = Linker->PreAnimatedState.FindStorage(FPreAnimatedCameraCutStorage::StorageID);
		FPreAnimatedStorageIndex StorageIndex = PreAnimatedStorage->FindStorageIndex(0);
		if (ensureMsgf(StorageIndex.IsValid(), TEXT("Blending camera back to gameplay but can't find pre-animated camera info!")))
		{
			FPreAnimatedCameraCutState CachedValue = PreAnimatedStorage->GetCachedValue(StorageIndex);
			CameraObject = CachedValue.LastViewTarget.ResolveObjectPtr();
			OverrideAspectRatioAxisConstraint = CachedValue.LastAspectRatioAxisConstraint;
		}
	}

	// If we find a camera component inside the provided object, let's make sure we are going to set
	// its owner as the next view target, and not some component (including the camera component itself).
	AActor* CameraActor = Cast<AActor>(CameraObject);
	UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);
	if (CameraComponent && CameraComponent->GetOwner() != CameraObject)
	{
		CameraObject = CameraComponent->GetOwner();
	}

	// If the view target isn't really changing, we don't have much to do.
	if (CameraObject == ViewTarget)
	{
		if (CameraCutParams.bJumpCut)
		{
			if (CameraManager)
			{
				CameraManager->SetGameCameraCutThisFrame();
			}

			if (CameraComponent)
			{
				CameraComponent->NotifyCameraCut();
			}

			if (UMovieSceneMotionVectorSimulationSystem* MotionVectorSim = Linker->FindSystem<UMovieSceneMotionVectorSimulationSystem>())
			{
				MotionVectorSim->SimulateAllTransforms();
			}
		}

		return;
	}

	// Time to set the camera cut! How we do it depends on whether we need to do some blending, or a straight cut.
	bool bDoSetViewTarget = true;
	FViewTargetTransitionParams TransitionParams;
	if (CameraCutParams.BlendType.IsSet())
	{
		UE_LOG(LogMovieScene, Log, TEXT("Blending into new camera cut: '%s' -> '%s' (blend time: %f)"),
			(ViewTarget ? *ViewTarget->GetName() : TEXT("None")),
			(CameraActor ? *CameraActor->GetName() : TEXT("None")),
			TransitionParams.BlendTime);

		const float PlayRateFactor = Wrapper.CameraCutCapability ? Wrapper.CameraCutCapability->GetCameraBlendPlayRate() : 1.f;

		// Convert known easing functions to their corresponding view target blend parameters.
		TTuple<EViewTargetBlendFunction, float> BlendFunctionAndExp = BuiltInEasingTypeToBlendFunction(CameraCutParams.BlendType.GetValue());
		TransitionParams.BlendTime = CameraCutParams.BlendTime * PlayRateFactor;
		TransitionParams.bLockOutgoing = CameraCutParams.bLockPreviousCamera;
		TransitionParams.BlendFunction = BlendFunctionAndExp.Get<0>();
		TransitionParams.BlendExp = BlendFunctionAndExp.Get<1>();

		// Calling SetViewTarget on a camera that we are currently transitioning to will 
		// result in that transition being aborted, and the view target being set immediately.
		// We want to avoid that, so let's leave the transition running if it's the case.
		const AActor* PendingViewTarget = CameraManager->PendingViewTarget.Target;
		if (CameraActor != nullptr && PendingViewTarget == CameraActor)
		{
			UE_LOG(LogMovieScene, Log, TEXT("Camera transition aborted, we are already blending towards the intended camera"));
			bDoSetViewTarget = false;
		}
	}
	else
	{
		UE_LOG(LogMovieScene, Log, TEXT("Starting new camera cut: '%s'"),
			(CameraActor ? *CameraActor->GetName() : TEXT("None")));
	}
	if (bDoSetViewTarget && ensureMsgf(CameraManager, TEXT("Can't set view target when there is no player controller!")))
	{
		CameraManager->SetViewTarget(CameraActor, TransitionParams);
	}

	// Override the aspect ratio constraint if this sequence requires it.
	ULocalPlayer* LocalPlayer = (PC != nullptr) ? PC->GetLocalPlayer() : nullptr;
	if (LocalPlayer != nullptr && OverrideAspectRatioAxisConstraint.IsSet())
	{
		LocalPlayer->AspectRatioAxisConstraint = OverrideAspectRatioAxisConstraint.GetValue();
	}

	// We want to notify of cuts on hard cuts and time jumps, but not on blend cuts.
	const bool bIsStraightCut = !CameraCutParams.BlendType.IsSet() || CameraCutParams.bJumpCut;

	if (CameraComponent && bIsStraightCut)
	{
		CameraComponent->NotifyCameraCut();
	}

	if (CameraManager)
	{
		CameraManager->bClientSimulatingViewTarget = (CameraActor != nullptr);

		if (bIsStraightCut)
		{
			CameraManager->SetGameCameraCutThisFrame();
		}
	}

	if (bIsStraightCut)
	{
		if (UMovieSceneMotionVectorSimulationSystem* MotionVectorSim = Linker->FindSystem<UMovieSceneMotionVectorSimulationSystem>())
		{
			MotionVectorSim->SimulateAllTransforms();
		}

		FOnCameraCutUpdatedParams CameraCutUpdatedParams;
		CameraCutUpdatedParams.ViewTarget = CameraActor;
		CameraCutUpdatedParams.ViewTargetCamera = CameraComponent;
		CameraCutUpdatedParams.bIsJumpCut = true;
		Wrapper.OnCameraCutUpdated(CameraCutUpdatedParams);
	}
}

}  // namespace UE::MovieScene

