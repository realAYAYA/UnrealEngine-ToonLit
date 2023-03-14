// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "EntitySystem/MovieScenePropertyBinding.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"
#include "Sections/TemplateSequenceSection.h"

struct FMovieSceneFloatChannel;

namespace UE
{
namespace MovieScene
{

/** Component data for how a template sequence should remap its object bindings to that of a parent object binding */
struct FTemplateSequenceComponentData
{
	FMovieSceneEvaluationOperand InnerOperand;
};

/** Component data for what property to scale inside a template sequence */
struct FTemplateSequencePropertyScaleComponentData
{
	/** Sequence ID where the property track lives */
	FMovieSceneSequenceID SubSequenceID;
	/** The object binding ID of the object for which to scale the given property */
	FGuid ObjectBinding;
	/** The property to scale */
	FMovieScenePropertyBinding PropertyBinding;
	/** The type of property scaling to do */
	ETemplateSectionPropertyScaleType PropertyScaleType;
};

struct FTemplateSequenceComponentTypes
{
public:
	static FTemplateSequenceComponentTypes* Get();

	TComponentTypeID<FTemplateSequenceComponentData> TemplateSequence;
	TComponentTypeID<FTemplateSequencePropertyScaleComponentData> PropertyScale;
	TComponentTypeID<FGuid> PropertyScaleReverseBindingLookup;

	struct
	{
		FComponentTypeID IsPropertyScaled;
	} Tags;

private:
	FTemplateSequenceComponentTypes();
};

} // namespace MovieScene
} // namespace UE

