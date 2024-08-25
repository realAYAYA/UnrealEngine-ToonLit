// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneByteChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneSection.h"
#include "DynamicMeshes/AvaShapeRectangleDynMesh.h"
#include "MovieSceneAvaShapeRectCornerSection.generated.h"

UCLASS(MinimalAPI)
class UMovieSceneAvaShapeRectCornerSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneAvaShapeRectCornerSection(const FObjectInitializer& ObjectInitializer);

	UPROPERTY()
	FMovieSceneByteChannel Type;

	UPROPERTY()
	FMovieSceneFloatChannel BevelSize;

	UPROPERTY()
	FMovieSceneByteChannel BevelSubdivisions;
};
