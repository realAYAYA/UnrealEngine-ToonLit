// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "CineCameraComponent.h"
#include "CameraAnimationSequence.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "UObject/WeakObjectPtr.h"
#include "SequenceCameraShake.generated.h"

class UCameraAnimationSequenceCameraStandIn;
class UMovieSceneEntitySystemLinker;
class UMovieSceneSequence;
class UCameraAnimationSequencePlayer;

/**
 * A camera shake pattern that plays a sequencer animation.
 */
UCLASS()
class TEMPLATESEQUENCE_API USequenceCameraShakePattern : public UCameraShakePattern
{
public:

	GENERATED_BODY()

	USequenceCameraShakePattern(const FObjectInitializer& ObjInit);

public:

	/** Source camera animation sequence to play. */
	UPROPERTY(EditAnywhere, Category=CameraShake)
	TObjectPtr<class UCameraAnimationSequence> Sequence;

	/** Scalar defining how fast to play the anim. */
	UPROPERTY(EditAnywhere, Category=CameraShake, meta=(ClampMin="0.001"))
	float PlayRate;

	/** Scalar defining how "intense" to play the anim. */
	UPROPERTY(EditAnywhere, Category=CameraShake, meta=(ClampMin="0.0"))
	float Scale;

	/** Linear blend-in time. */
	UPROPERTY(EditAnywhere, Category=CameraShake, meta=(ClampMin="0.0"))
	float BlendInTime;

	/** Linear blend-out time. */
	UPROPERTY(EditAnywhere, Category=CameraShake, meta=(ClampMin="0.0"))
	float BlendOutTime;

	/** When bRandomSegment is true, defines how long the sequence should play. */
	UPROPERTY(EditAnywhere, Category=CameraShake, meta=(ClampMin="0.0", EditCondition="bRandomSegment"))
	float RandomSegmentDuration;

	/**
	 * When true, plays a random snippet of the sequence for RandomSegmentDuration seconds.
	 *
	 * @note The sequence we be forced to loop when bRandomSegment is enabled, in case the duration
	 *       is longer than what's left to play from the random start time.
	 */
	UPROPERTY(EditAnywhere, Category=CameraShake)
	bool bRandomSegment;

private:

	// UCameraShakeBase interface
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakeStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult) override;
	virtual void ScrubShakePatternImpl(const FCameraShakeScrubParams& Params, FCameraShakeUpdateResult& OutResult) override;
	virtual void StopShakePatternImpl(const FCameraShakeStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

	void UpdateCamera(FFrameTime NewPosition, const FMinimalViewInfo& InPOV, FCameraShakeUpdateResult& OutResult);

private:

	/** The player we use to play the camera animation sequence */
	UPROPERTY(Instanced, Transient)
	TObjectPtr<UCameraAnimationSequencePlayer> Player;

	/** Standin for the camera actor and components */
	UPROPERTY(Instanced, Transient)
	TObjectPtr<UCameraAnimationSequenceCameraStandIn> CameraStandIn;
};
