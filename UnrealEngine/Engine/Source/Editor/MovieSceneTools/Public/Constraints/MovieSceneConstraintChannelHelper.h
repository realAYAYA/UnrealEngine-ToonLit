// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "MovieSceneSection.h"


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
struct FConstraintAndActiveChannel;
enum class EMovieSceneKeyInterpolation : uint8;

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
	void ComputeCurrentTransforms(UWorld* InWorld);
	
private:

	const TArray< TWeakObjectPtr<UTickableConstraint> > GetHandleTransformConstraints(UWorld* InWorld) const;

	UTickableTransformConstraint* Constraint = nullptr;
	UTransformableHandle* Handle = nullptr;
};

struct MOVIESCENETOOLS_API FConstraintSections
{
	UMovieSceneSection* ConstraintSection = nullptr;
	UMovieSceneSection* ChildTransformSection = nullptr;
	UMovieSceneSection* ParentTransformSection = nullptr;
	FConstraintAndActiveChannel* ActiveChannel = nullptr;
	ITransformConstraintChannelInterface* Interface = nullptr;
};
struct MOVIESCENETOOLS_API FMovieSceneConstraintChannelHelper
{
public:

	static bool AddConstraintToSequencer(
		const TSharedPtr<ISequencer>& InSequencer,
		UTickableTransformConstraint* InConstraint);
	
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

	/* Get the section and the channel for the given constraint, will be nullptr's if it doesn't exist in Sequencer*/
	static FConstraintSections  GetConstraintSectionAndChannel(
		const UTickableTransformConstraint* InConstraint,
		const TSharedPtr<ISequencer>& InSequencer);

	/* For the given constraint get all of the transform keys for it's child and parent handles*/
	static void GetTransformFramesForConstraintHandles(
		const UTickableTransformConstraint* InConstraint,
		const TSharedPtr<ISequencer>& InSequencer,
		const FFrameNumber& StartFrame,
		const FFrameNumber& EndFrame,
		TArray<FFrameNumber>& OutFramesToBake);


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

	/** delete transform keys at that time */
	template< typename ChannelType >
	static void DeleteTransformKeys(
		const TArrayView<ChannelType*>& InChannels,
		const FFrameNumber& InTime);

	/** Change key interpolation at the specified time*/
	template< typename ChannelType >
	static void ChangeKeyInterpolation(
		const TArrayView<ChannelType*>& InChannels,
		const FFrameNumber& InTime,
		EMovieSceneKeyInterpolation KeyInterpolation);

	static void HandleConstraintPropertyChanged(
		UTickableTransformConstraint* InConstraint,
		const FMovieSceneConstraintChannel& InActiveChannel,
		const FPropertyChangedEvent& InPropertyChangedEvent,
		const TSharedPtr<ISequencer>& InSequencer,
		UMovieSceneSection* InSection);

	template< typename ChannelType >
	static TArray<FFrameNumber> GetTransformTimes(
		const TArrayView<ChannelType*>& InChannels,
		const FFrameNumber& StartTime, 
		const FFrameNumber& EndTime);

	template< typename ChannelType >
	static  void DeleteTransformTimes(
		const TArrayView<ChannelType*>& InChannels,
		const FFrameNumber& StartTime,
		const FFrameNumber& EndTime,
		EMovieSceneTransformChannel Channels = EMovieSceneTransformChannel::AllTransform);

	/** this will only set the value son channels with keys at the specified time, reusing tangent time etc. */
	template< typename ChannelType >
	static void SetTransformTimes(
		const TArrayView<ChannelType*>& InChannels,
		const TArray<FFrameNumber>& Frames,
		const TArray<FTransform>& Transforms);

	static bool bDoNotCompensate;

private:
	/** Get the animatable interface for that handle if registered. */
	static ITransformConstraintChannelInterface* GetHandleInterface(const UTransformableHandle* InHandle);
	
	/** For the given handle create any movie scene binding for it based upon the current sequencer that's open*/
	static void CreateBindingIDForHandle(const TSharedPtr<ISequencer>& InSequencer, UTransformableHandle* InHandle);

	/** Compensate scale keys when enabling/disabling scaling for parent constraints. */
	static void CompensateScale(
		UTickableParentConstraint* InParentConstraint,
		const FMovieSceneConstraintChannel& InActiveChannel,
		const TSharedPtr<ISequencer>& InSequencer,
		UMovieSceneSection* InSection);

	/** Handle offset modifications so that the child's transform channels are synced. */
	static void HandleOffsetChanged(
		UTickableTransformConstraint* InConstraint,
		const FMovieSceneConstraintChannel& InActiveChannel,
		const TSharedPtr<ISequencer>& InSequencer);
};
