// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"
#include "Engine/Scene.h"
#include "TemplateSequencePlayer.h"
#include "CameraAnimationSequencePlayer.generated.h"

/**
 * A dummy class that we give to a sequence in lieu of an actual camera actor.
 */
UCLASS()
class TEMPLATESEQUENCE_API UCameraAnimationSequenceCameraStandIn : public UObject
{
public:

	GENERATED_BODY()

	UCameraAnimationSequenceCameraStandIn(const FObjectInitializer& ObjInit);

public:
	/**
	 * Scene component properties
	 *
	 * Transform doesn't need to be a UPROPERTY because we register a custom getter/setter. This is
	 * because the sequence runtime expects that in order to use the intermediate transform struct.
	 */
	const FTransform& GetTransform() const { return Transform; }
	void SetTransform(const FTransform& InTransform) { Transform = InTransform; }

	/** Camera component properties */
	UPROPERTY()
	float FieldOfView;

	UPROPERTY()
	uint8 bConstrainAspectRatio : 1;

	UPROPERTY()
	float AspectRatio;

	UPROPERTY()
	FPostProcessSettings PostProcessSettings;

	UPROPERTY()
	float PostProcessBlendWeight;

	/** Cine camera component properties */
	UPROPERTY()
	FCameraFilmbackSettings Filmback;

	UPROPERTY()
	FCameraLensSettings LensSettings;

	UPROPERTY()
	FCameraFocusSettings FocusSettings;

	UPROPERTY()
	float CurrentFocalLength;

	UPROPERTY()
	float CurrentAperture;

	UPROPERTY()
	float CurrentFocusDistance;

public:
	/** Initialize this object's properties based on the given sequence's root object template */
	void Initialize(UTemplateSequence* TemplateSequence);

	/** Reset the properties of the stand-in every frame before animation */
	void Reset(const FMinimalViewInfo& ViewInfo, UMovieSceneEntitySystemLinker* Linker);

	/** Recompute camera and lens settings after each frame */
	void RecalcDerivedData();

	/** Register the stand-in class with the sequencer ECS component registry */
	static void RegisterCameraStandIn();

	/** Unregister the stand-in class with the sequencer ECS component registry */
	static void UnregisterCameraStandIn();

private:
	void ResetDefaultValues(const FMinimalViewInfo& ViewInfo);
	void UpdateInitialPropertyValues(UMovieSceneEntitySystemLinker* Linker);

private:
	static bool bRegistered;

	FTransform Transform;

	bool bIsCineCamera = false;
	float WorldToMeters = 0.f;
};

/**
 * A lightweight sequence player for playing camera animation sequences.
 */
UCLASS()
class TEMPLATESEQUENCE_API UCameraAnimationSequencePlayer
	: public UObject
	, public IMovieScenePlayer
{
public:
	GENERATED_BODY()

	UCameraAnimationSequencePlayer(const FObjectInitializer& ObjInit);
	virtual ~UCameraAnimationSequencePlayer();

	/** Initializes this player with the given sequence */
	void Initialize(UMovieSceneSequence* InSequence);
	/** Start playing the sequence */
	void Play(bool bLoop = false, bool bRandomStartTime = false);
	/** Advance play to the given time */
	void Update(FFrameTime NewPosition);
	/** Jumps to the given time */
	void Jump(FFrameTime NewPosition);
	/** Stop playing the sequence */
	void Stop();

	/** Sets the player in scrub mode */
	void StartScrubbing();
	/** Ends scrub mode */
	void EndScrubbing();

	/** Gets whether playback is looping */
	bool GetIsLooping() const { return bIsLooping; }
	/** Gets the current play position */
	FFrameTime GetCurrentPosition() const { return PlayPosition.GetCurrentPosition(); }
	/** Get the sequence display resolution */
	FFrameRate GetInputRate() const { return PlayPosition.GetInputRate(); }
	/** Get the sequence tick resolution */
	FFrameRate GetOutputRate() const { return PlayPosition.GetOutputRate(); }
	/** Get the duration of the current sequence */
	FFrameNumber GetDuration() const;

	/** Sets an object that can be used to bind everything in the sequence */
	void SetBoundObjectOverride(UObject* InObject);

public:

	// IMovieScenePlayer interface
	virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() override { return RootTemplateInstance; }
	virtual UMovieSceneEntitySystemLinker* ConstructEntitySystemLinker() override;
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual UObject* AsUObject() override { return this; }
	virtual FMovieSceneSpawnRegister& GetSpawnRegister() override { return SpawnRegister; }

	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override {}
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override {}
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override {}
	virtual bool CanUpdateCameraCut() const override { return false; }
	virtual void UpdateCameraCut(UObject* CameraObject, const EMovieSceneCameraCutParams& CameraCutParams) override {}
	virtual void ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID InSequenceID, UMovieSceneSequence& InSequence, UObject* InResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;

	// UObject interface 
	virtual bool IsDestructionThreadSafe() const override { return false; }
	virtual void BeginDestroy() override;

private:

	FSequenceCameraShakeSpawnRegister SpawnRegister;

	/** Bound object overrides */
	UPROPERTY(transient)
	TObjectPtr<UObject> BoundObjectOverride;

	/** The sequence to play back */
	UPROPERTY(transient)
	TObjectPtr<UMovieSceneSequence> Sequence;

	/** The evaluation template instance */
	UPROPERTY(transient)
	FMovieSceneRootEvaluationTemplateInstance RootTemplateInstance;

	/** Play position helper */
	FMovieScenePlaybackPosition PlayPosition;

	/** Start frame for the sequence */
	FFrameNumber StartFrame;

	/** The sequence duration in frames */
	FFrameNumber DurationFrames;

	/** Whether we should be looping */
	bool bIsLooping;

	/** Movie player status. */
	TEnumAsByte<EMovieScenePlayerStatus::Type> Status;
};

