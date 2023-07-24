// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "Evaluation/MovieScenePlayback.h"
#include "UObject/ObjectKey.h"
#include "MovieSceneSkeletalAnimationSystem.generated.h"

class IMovieScenePlayer;
class UAnimMontage;
class UMovieSceneSkeletalAnimationSection;

namespace UE::MovieScene
{

/** Information for a single skeletal animation playing on a bound object */
struct FActiveSkeletalAnimation
{
	IMovieScenePlayer* Player;
	const UMovieSceneSkeletalAnimationSection* AnimSection;
	FMovieSceneContext Context;
	FMovieSceneEntityID EntityID;
	FRootInstanceHandle RootInstanceHandle;
	float FromEvalTime;
	float ToEvalTime;
	double BlendWeight;
	bool bWantsRestoreState;
};

/** Information for all skeletal animations playing on a bound object */
struct FBoundObjectActiveSkeletalAnimations
{
	using FAnimationArray = TArray<FActiveSkeletalAnimation, TInlineAllocator<2>>;

	/** All active animations on the corresponding bound object */
	FAnimationArray Animations;
	/** Motion vector simulation animations on the corresponding bound object */
	FAnimationArray SimulatedAnimations;
};

/** Temporary information about montage setups. */
struct FMontagePlayerPerSectionData 
{
	TWeakObjectPtr<UAnimMontage> Montage;
	int32 MontageInstanceId;
};

struct FSkeletalAnimationSystemData
{
	/** Map of active skeletal animations for each bound object */
	TMap<UObject*, FBoundObjectActiveSkeletalAnimations> SkeletalAnimations;

	/** Map of persistent montage data */
	TMap<FObjectKey, FMontagePlayerPerSectionData> MontageData;
};

} // namespace UE::MovieScene

UCLASS(MinimalAPI)
class UMovieSceneSkeletalAnimationSystem
	: public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneSkeletalAnimationSystem(const FObjectInitializer& ObjInit);

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override final;

	void CleanSystemData();

private:

	UE::MovieScene::FSkeletalAnimationSystemData SystemData;
};

