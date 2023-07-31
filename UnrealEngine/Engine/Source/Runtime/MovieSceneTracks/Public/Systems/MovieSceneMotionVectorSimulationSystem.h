// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "Math/Transform.h"
#include "Containers/Map.h"
#include "UObject/ObjectKey.h"
#include "UObject/NameTypes.h"

#include "MovieSceneMotionVectorSimulationSystem.generated.h"

struct FMovieSceneAnimTypeID;

namespace UE
{
namespace MovieScene
{


FFrameTime GetSimulatedMotionVectorTime(const FMovieSceneContext& Context);


} // namespace MovieScene
} // namespace UE

UCLASS()
class MOVIESCENETRACKS_API UMovieSceneMotionVectorSimulationSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneMotionVectorSimulationSystem(const FObjectInitializer& ObjInit);

	void EnableThisFrame();
	void SimulateAllTransforms();
	bool IsSimulationEnabled() const
	{
		return bSimulationEnabled;
	}

	void PreserveSimulatedMotion(bool bShouldPreserveTransforms);

	void AddSimulatedTransform(USceneComponent* Component, const FTransform& SimulatedTransform, FName SocketName);

private:

	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override final;

	void OnPostEvaluation();

	void ComputeSimulatedMotion();

private:

	void PropagateMotionToComponents();

	FTransform GetTransform(USceneComponent* Component);

	FTransform GetSocketTransform(USceneComponent* Component, FName SocketName);

	bool HavePreviousTransformForParent(USceneComponent* InComponent) const;

	void ApplySimulatedTransforms(USceneComponent* InComponent, const FTransform& InPreviousTransform);

private:

	/** Transform data relating to a specific object or socket */
	struct FSimulatedTransform
	{
		FSimulatedTransform(const FTransform& InTransform, FName InSocketName = NAME_None)
			: Transform(InTransform), SocketName(InSocketName)
		{}

		/** The transform for the object */
		FTransform Transform;

		/** The socket name to which this relates */
		FName SocketName;
	};

	/** Map of object -> previous transform data */
	TMultiMap<FObjectKey, FSimulatedTransform> TransformData;

	/** Whether to reset TransformData at the end of the frame or not */
	bool bPreserveTransforms = false;

	bool bSimulationEnabled = false;

	bool bSimulateTransformsRequested = false;
};



