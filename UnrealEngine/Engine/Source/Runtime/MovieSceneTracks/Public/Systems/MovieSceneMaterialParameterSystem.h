// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneOverlappingEntityTracker.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateStorage.h"

#include "MovieSceneMaterialParameterSystem.generated.h"

class UMovieSceneBlenderSystem;
class UMovieScenePiecewiseDoubleBlenderSystem;

namespace UE::MovieScene
{

struct FAnimatedMaterialParameterInfo
{
	/** Weak linker ptr - only assigned if the output entity is ever allocated */
	TWeakObjectPtr<UMovieSceneEntitySystemLinker> WeakLinker;
	/** Weak blender system ptr - only assigned if the blend channel is ever allocated */
	TWeakObjectPtr<UMovieSceneBlenderSystem> WeakBlenderSystem;
	int32 NumContributors = 0;
	FMovieSceneEntityID OutputEntityID;
	FMovieSceneBlendChannelID BlendChannelID;

	~FAnimatedMaterialParameterInfo();
};

MOVIESCENETRACKS_API void CollectGarbageForOutput(FAnimatedMaterialParameterInfo* Output);

} // namespace UE::MovieScene


/**
 * System responsible for tracking and animating material parameter entities.
 * Operates on the following component types from FMovieSceneTracksComponentTypes:
 *
 * Instantiation: Tracks any BoundMaterial with a ScalarParameterName, ColorParameterName or VectorParameterName.
 *                Manages adding BlendChannelInputs and Outputs where multiple entities animate the same parameter
 *                on the same bound material.
 *                BoundMaterials may be a UMaterialInstanceDynamic, or a UMaterialParameterCollectionInstance.
 *
 * Evaluation:    Visits any BoundMaterial with the supported parameter names and either a BlendChannelOutput component
 *                or no BlendChannelInput, and applies the resulting parameter to the bound material instance.
 */
UCLASS(MinimalAPI)
class UMovieSceneMaterialParameterSystem
	: public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneMaterialParameterSystem(const FObjectInitializer& ObjInit);

private:

	virtual void OnLink() override;
	virtual void OnUnlink() override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

	void OnInstantiation();
	void OnEvaluation(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents);

private:

	/** Overlapping trackers that track multiple entities animating the same bound object and name */
	UE::MovieScene::TOverlappingEntityTracker<UE::MovieScene::FAnimatedMaterialParameterInfo, UObject*, FName> ScalarParameterTracker;
	UE::MovieScene::TOverlappingEntityTracker<UE::MovieScene::FAnimatedMaterialParameterInfo, UObject*, FName> VectorParameterTracker;

public:

	UPROPERTY()
	TObjectPtr<UMovieScenePiecewiseDoubleBlenderSystem> DoubleBlenderSystem;
};
