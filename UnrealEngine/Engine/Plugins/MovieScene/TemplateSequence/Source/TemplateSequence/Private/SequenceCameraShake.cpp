// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequenceCameraShake.h"
#include "Algo/IndexOf.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CameraAnimationSequencePlayer.h"
#include "CineCameraActor.h"
#include "Containers/ArrayView.h"
#include "Engine/World.h"
#include "EntitySystem/MovieSceneBoundSceneComponentInstantiator.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieScenePropertySystemTypes.h"
#include "GameFramework/WorldSettings.h"
#include "MovieSceneFwd.h"
#include "MovieSceneTimeHelpers.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "MovieSceneTracksComponentTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SequenceCameraShake)

#if !IS_MONOLITHIC
	UE::MovieScene::FEntityManager*& GEntityManagerForDebugging = UE::MovieScene::GEntityManagerForDebuggingVisualizers;
#endif

USequenceCameraShakePattern::USequenceCameraShakePattern(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, PlayRate(1.f)
	, Scale(1.f)
	, BlendInTime(0.2f)
	, BlendOutTime(0.4f)
	, RandomSegmentDuration(0.f)
	, bRandomSegment(false)
{
	CameraStandIn = CreateDefaultSubobject<UCameraAnimationSequenceCameraStandIn>(TEXT("CameraStandIn"), true);
	Player = CreateDefaultSubobject<UCameraAnimationSequencePlayer>(TEXT("CameraShakePlayer"), true);
}

void USequenceCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	if (Sequence != nullptr)
	{
		if (UMovieScene* MovieScene = Sequence->GetMovieScene())
		{
			const float ActualPlayRate = (PlayRate > 0.f) ? PlayRate : 1.f;

			if (bRandomSegment)
			{
				OutInfo.Duration = FCameraShakeDuration(RandomSegmentDuration / ActualPlayRate);
			}
			else
			{
				const FFrameRate TickResolution = MovieScene->GetTickResolution();
				const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
				const float Duration = TickResolution.AsSeconds(PlaybackRange.Size<FFrameNumber>());
				OutInfo.Duration = FCameraShakeDuration(Duration / ActualPlayRate);
			}

			OutInfo.BlendIn = BlendInTime;
			OutInfo.BlendOut = BlendOutTime;
		}
	}
}

void USequenceCameraShakePattern::StartShakePatternImpl(const FCameraShakeStartParams& Params)
{
	using namespace UE::MovieScene;

	if (!ensure(Sequence))
	{
		return;
	}

	// Initialize our stand-in object.
	CameraStandIn->Initialize(Sequence);
	
	// Make the player always use our stand-in object whenever a sequence wants to spawn or possess an object.
	Player->SetBoundObjectOverride(CameraStandIn);

	// Initialize it and start playing.
	Player->Initialize(Sequence);
	Player->Play(bRandomSegment, bRandomSegment);
}

void USequenceCameraShakePattern::UpdateShakePatternImpl(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult)
{
	const FFrameRate InputRate = Player->GetInputRate();
	const FFrameTime NewPosition = Player->GetCurrentPosition() + Params.DeltaTime * PlayRate * InputRate;
	UpdateCamera(NewPosition, Params.POV, OutResult);
}

void USequenceCameraShakePattern::ScrubShakePatternImpl(const FCameraShakeScrubParams& Params, FCameraShakeUpdateResult& OutResult)
{
	Player->StartScrubbing();
	{
		const FFrameRate InputRate = Player->GetInputRate();
		const FFrameTime NewPosition = Params.AbsoluteTime * PlayRate * InputRate;
		UpdateCamera(NewPosition, Params.POV, OutResult);
	}
	Player->EndScrubbing();
}

void USequenceCameraShakePattern::StopShakePatternImpl(const FCameraShakeStopParams& Params)
{
	using namespace UE::MovieScene;

	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (ensure(Player != nullptr && MovieScene != nullptr))
	{
		if (Params.bImmediately)
		{
			// Stop playing!
			Player->Stop();
		}
		else
		{
			// Move the playback position to the start of the blend out.
			const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
			if (PlaybackRange.HasUpperBound())
			{
				const FFrameRate InputRate = Player->GetInputRate();
				const FFrameTime BlendOutTimeInFrames  = BlendOutTime * InputRate;
				const FFrameTime BlendOutStartFrame = FFrameTime(PlaybackRange.GetUpperBoundValue()) - BlendOutTimeInFrames;
				Player->Jump(BlendOutStartFrame);
			}
		}
	}
}

void USequenceCameraShakePattern::TeardownShakePatternImpl()
{
	using namespace UE::MovieScene;

	// Stop if we had reached the end of the animation and the sequence needs finishing.
	// If the shake had been stopped explicitly, this basically won't do anything.
	Player->Stop();
}

void USequenceCameraShakePattern::UpdateCamera(FFrameTime NewPosition, const FMinimalViewInfo& InPOV, FCameraShakeUpdateResult& OutResult)
{
	using namespace UE::MovieScene;

	check(CameraStandIn);
	check(Player);

	UMovieSceneEntitySystemLinker* Linker = Player->GetEvaluationTemplate().GetEntitySystemLinker();
	CameraStandIn->Reset(InPOV, Linker);

	// Get the "unshaken" properties that need to be treated additively.
	const float OriginalFieldOfView = CameraStandIn->FieldOfView;

	// Update the sequence.
	Player->Update(NewPosition);

	// Recalculate properties that might be invalidated by other properties having been animated.
	CameraStandIn->RecalcDerivedData();

	// Grab the final animated (shaken) values, figure out the delta, apply scale, and feed that into the 
	// camera shake result.
	// Transform is always treated as a local, additive value. The data better be good.
	const FTransform ShakenTransform = CameraStandIn->GetTransform();
	OutResult.Location = ShakenTransform.GetLocation() * Scale;
	OutResult.Rotation = ShakenTransform.GetRotation().Rotator() * Scale;

	// FieldOfView follows the current camera's value every frame, so we can compute how much the shake is
	// changing it.
	const float ShakenFieldOfView = CameraStandIn->FieldOfView;
	const float DeltaFieldOfView = ShakenFieldOfView - OriginalFieldOfView;
	OutResult.FOV = DeltaFieldOfView * Scale;

	// The other properties aren't treated as additive.
	OutResult.PostProcessSettings = CameraStandIn->PostProcessSettings;
	OutResult.PostProcessBlendWeight = CameraStandIn->PostProcessBlendWeight;
}


