// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FChannelMapInfo;
struct FConstraintAndActiveChannel;
class UControlRig;
class ISequencer;
class UMovieScene3DTransformSection;
class UMovieSceneControlRigParameterSection;
class IMovieSceneConstrainedSection;
class UMovieSceneSection;
class UTransformableHandle;
class UTickableTransformConstraint;
class UTransformableControlHandle;
class UTransformableComponentHandle;
struct FMovieSceneFloatChannel;
struct FMovieSceneDoubleChannel;
struct FMovieSceneConstraintChannel;
struct FFrameNumber;

/**
 * 
 */

struct FConstraintChannelHelper
{
public:
	/** @todo documentation. */
	static bool IsKeyframingAvailable();
	
	/** Adds an active key if needed and does the compensation when switching. */
	static void SmartConstraintKey(UTickableTransformConstraint* InConstraint);


	/** Compensate transform on handles when a constraint switches state. */
	static void Compensate(
		UTickableTransformConstraint* InConstraint,
		const bool bAllTimes = false);
	static void CompensateIfNeeded(
		UWorld* InWorld,
		const TSharedPtr<ISequencer>& InSequencer,
		IMovieSceneConstrainedSection* Section,
		const TOptional<FFrameNumber>& OptionalTime);

	/** @todo documentation. */
	static UMovieSceneControlRigParameterSection* GetControlSection(
		const UTransformableControlHandle* InHandle,
		const TSharedPtr<ISequencer>& InSequencer);

private:

	/** BEGIN CONTROL RIG SECTION */
	
	/** @todo documentation. */
	static void SmartControlConstraintKey(
		UTickableTransformConstraint* InConstraint,
		const TSharedPtr<ISequencer>& InSequencer);
	
	/** END CONTROL RIG SECTION */

	/** BEGIN COMPONENT SECTION */

	/** @todo documentation. */
	static void SmartComponentConstraintKey(
		UTickableTransformConstraint* InConstraint,
		const TSharedPtr<ISequencer>& InSequencer);

	/** @todo documentation. */
	static UMovieScene3DTransformSection* GetTransformSection(
		const UTransformableComponentHandle* InHandle,
		const TSharedPtr<ISequencer>& InSequencer);

	/** END COMPONENT SECTION */

	/** For the given handle create any movie scene binding for it based upon the current sequencer that's open*/
	static void CreateBindingIDForHandle(UTransformableHandle* InHandle);

	
};
