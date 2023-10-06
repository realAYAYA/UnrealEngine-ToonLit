// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameNumber.h"
#include "Templates/SharedPointer.h"
#include "BakingAnimationKeySettings.h"

class UMovieSceneSection;
class UTransformableHandle;
class UTickableTransformConstraint;
class ISequencer;
class UWorld;
struct FFrameNumber;
struct FConstraintBaker
{
public:
	/** Bake constraint over specified frames, frames must be in order*/
	static void Bake(
		UWorld* InWorld,
		UTickableTransformConstraint* InConstraint,
		const TSharedPtr<ISequencer>& InSequencer,
		const TOptional< FBakingAnimationKeySettings>& InSettings,
		const TOptional<TArray<FFrameNumber>>& InFrames);
	
	/** Bake Multiple Constraints using settings*/
	static bool BakeMultiple(
		UWorld* InWorld,
		TArray< UTickableTransformConstraint*>& InConstraints,
		const TSharedPtr<ISequencer>& InSequencer,
		const FBakingAnimationKeySettings& Settings);
	
private:

	/** Computes the minimal frames to bake for that constraint. */
	static void GetMinimalFramesToBake(
		UWorld* InWorld,
		const UTickableTransformConstraint* InConstraint,
		const TSharedPtr<ISequencer>& InSequencer,
		UMovieSceneSection* InSection,
		const TOptional< FBakingAnimationKeySettings>& Settings,
		TArray<FFrameNumber>& OutFramesToBake);
};

