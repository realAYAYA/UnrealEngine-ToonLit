// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"

class IMovieSceneConstrainedSection;
struct ITransformConstraintChannelInterface;
class UWorld;
class UTickableConstraint;
class ISequencer;
class UMovieSceneSection;
class UTickableTransformConstraint;
class UTickableParentConstraint;
class UTransformableHandle;
struct FMovieSceneConstraintChannel;
struct FFrameNumber;


struct MOVIESCENETOOLS_API FCompensationEvaluator
{
public:
	TArray<FTransform> ChildLocals;
	TArray<FTransform> ChildGlobals;
	TArray<FTransform> SpaceGlobals;

	FCompensationEvaluator(UTickableTransformConstraint* InConstraint);

	void ComputeLocalTransforms(UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer, const TArray<FFrameNumber>& InFrames, const bool bToActive);
	void ComputeLocalTransformsBeforeDeletion(UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer, const TArray<FFrameNumber>& InFrames);
	void ComputeCompensation(UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer, const FFrameNumber& InTime);
	void ComputeLocalTransformsForBaking(UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer, const TArray<FFrameNumber>& InFrames);
	void CacheTransforms(UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer, const TArray<FFrameNumber>& InFrames);
	
private:

	TArray< TObjectPtr<UTickableConstraint> > GetHandleTransformConstraints(UWorld* InWorld) const;

	UTickableTransformConstraint* Constraint = nullptr;
	UTransformableHandle* Handle = nullptr;
};

struct MOVIESCENETOOLS_API FMovieSceneConstraintChannelHelper
{
public:

	/** Adds an active key if needed and does the compensation when switching. Will use the optional active and time if set. 
	Will return true if key is actually set, may not be if the value is the same.*/
	static bool SmartConstraintKey(
		const TSharedPtr<ISequencer>& InSequencer,
		UTickableTransformConstraint* InConstraint, 
		const TOptional<bool>& InOptActive,
		const TOptional<FFrameNumber>& InOptFrameTime);
	
	/** Compensate transform on handles when a constraint switches state. */
	static void Compensate(
		const TSharedPtr<ISequencer>& InSequencer,
		const UTickableTransformConstraint* InConstraint,
		const TOptional<FFrameNumber>& InOptTime);
	
	static void CompensateIfNeeded(
		const TSharedPtr<ISequencer>& InSequencer,
		IMovieSceneConstrainedSection* Section,
		const TOptional<FFrameNumber>& OptionalTime,
		const int32 InChildHash = INDEX_NONE);
		
	static void HandleConstraintRemoved(
		UTickableConstraint* InConstraint,
		const FMovieSceneConstraintChannel* InChannel,
		const TSharedPtr<ISequencer>& InSequencer,
		UMovieSceneSection* InSection);

	static void HandleConstraintKeyDeleted(
		UTickableTransformConstraint* InConstraint,
		const FMovieSceneConstraintChannel* InConstraintChannel,
		const TSharedPtr<ISequencer>& InSequencer,
		UMovieSceneSection* InSection,
		const FFrameNumber& InTime);

	static void HandleConstraintKeyMoved(
		const UTickableTransformConstraint* InConstraint,
		const FMovieSceneConstraintChannel* InConstraintChannel,
		UMovieSceneSection* InSection,
		const FFrameNumber& InCurrentFrame, const FFrameNumber& InNextFrame);

	/** @todo documentation. */
	template<typename ChannelType>
	static void GetFramesToCompensate(
		const FMovieSceneConstraintChannel& InActiveChannel,
		const bool InActiveValueToBeSet,
		const FFrameNumber& InTime,
		const TArrayView<ChannelType*>& InChannels,
		TArray<FFrameNumber>& OutFramesAfter);

	/** @todo documentation. */
	template< typename ChannelType >
	static void GetFramesAfter(
		const FMovieSceneConstraintChannel& InActiveChannel,
		const FFrameNumber& InTime,
		const TArrayView<ChannelType*>& InChannels,
		TArray<FFrameNumber>& OutFrames);

	/** @todo documentation. */
	template< typename ChannelType >
	static void GetFramesWithinActiveState(
		const FMovieSceneConstraintChannel& InActiveChannel,
		const TArrayView<ChannelType*>& InChannels,
		TArray<FFrameNumber>& OutFrames);

	/** @todo documentation. */
	template< typename ChannelType >
	static void MoveTransformKeys(
		const TArrayView<ChannelType*>& InChannels,
		const FFrameNumber& InCurrentTime,
		const FFrameNumber& InNextTime);

	/** @todo documentation. */
	template< typename ChannelType >
	static void DeleteTransformKeys(
		const TArrayView<ChannelType*>& InChannels,
		const FFrameNumber& InTime);

	static void HandleConstraintPropertyChanged(
		UTickableTransformConstraint* InConstraint,
		const FMovieSceneConstraintChannel& InActiveChannel,
		const FPropertyChangedEvent& InPropertyChangedEvent,
		const TSharedPtr<ISequencer>& InSequencer,
		UMovieSceneSection* InSection);

	static bool bDoNotCompensate;

private:
	/** Get the animatable interface for that handle if registered. */
	static ITransformConstraintChannelInterface* GetHandleInterface(const UTransformableHandle* InHandle);
	
	/** For the given handle create any movie scene binding for it based upon the current sequencer that's open*/
	static void CreateBindingIDForHandle(const TSharedPtr<ISequencer>& InSequencer, UTransformableHandle* InHandle);

	static void CompensateScale(
		UTickableParentConstraint* InParentConstraint,
		const FMovieSceneConstraintChannel& InActiveChannel,
		const TSharedPtr<ISequencer>& InSequencer,
		UMovieSceneSection* InSection);
};
