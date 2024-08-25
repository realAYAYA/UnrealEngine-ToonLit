// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FChannelMapInfo;
class UControlRig;
class UMovieSceneControlRigParameterSection;
struct FMovieSceneFloatChannel;
struct FMovieSceneBoolChannel;
struct FMovieSceneIntegerChannel;
struct FMovieSceneByteChannel;
class UMovieSceneSection;
class UMovieSceneControlRigParameterTrack;
class UMovieSceneSequence;

struct CONTROLRIG_API FControlRigSequencerHelpers
{
	static TPair<const FChannelMapInfo*, int32> GetInfoAndNumFloatChannels(
		const UControlRig* InControlRig,
		const FName& InControlName,
		const UMovieSceneControlRigParameterSection* InSection);

	static TArrayView<FMovieSceneFloatChannel*> GetFloatChannels(const UControlRig* InControlRig,
		const FName& InControlName, const UMovieSceneSection* InSection);

	static TArrayView<FMovieSceneBoolChannel*> GetBoolChannels(const UControlRig* InControlRig,
		const FName& InControlName, const UMovieSceneSection* InSection);

	static TArrayView<FMovieSceneByteChannel*> GetByteChannels(const UControlRig* InControlRig,
		const FName& InControlName, const UMovieSceneSection* InSection);

	static TArrayView<FMovieSceneIntegerChannel*> GetIntegerChannels(const UControlRig* InControlRig,
		const FName& InControlName, const UMovieSceneSection* InSection);

	static UMovieSceneControlRigParameterTrack*  FindControlRigTrack(UMovieSceneSequence* InSequencer, const UControlRig* InControlRig);

};

