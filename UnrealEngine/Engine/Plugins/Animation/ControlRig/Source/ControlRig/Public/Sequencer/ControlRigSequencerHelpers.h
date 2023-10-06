// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FChannelMapInfo;
class UControlRig;
class UMovieSceneControlRigParameterSection;
struct FMovieSceneFloatChannel;
class UMovieSceneSection;
class ISequencer;

struct CONTROLRIG_API FControlRigSequencerHelpers
{
	static TPair<const FChannelMapInfo*, int32> GetInfoAndNumFloatChannels(
		const UControlRig* InControlRig,
		const FName& InControlName,
		const UMovieSceneControlRigParameterSection* InSection);

	static TArrayView<FMovieSceneFloatChannel*> GetFloatChannels(const UControlRig* InControlRig,
		const FName& InControlName, const UMovieSceneSection* InSection);

	static UMovieSceneControlRigParameterSection* GetControlRigSection(ISequencer* InSequencer, const UControlRig* InControlRig);

};

