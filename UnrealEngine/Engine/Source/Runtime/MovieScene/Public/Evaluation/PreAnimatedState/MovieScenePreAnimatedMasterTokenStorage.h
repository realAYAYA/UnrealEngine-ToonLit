// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateTypes.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.h"
#include "Evaluation/PreAnimatedState/MovieSceneRestoreStateParams.h"
#include "Misc/InlineValue.h"
#include "MovieSceneExecutionToken.h"


namespace UE
{
namespace MovieScene
{
struct FPreAnimatedStateExtension;
struct FRestoreStateParams;

struct FPreAnimatedMasterTokenTraits : FDefaultPreAnimatedStateTraits
{
	using KeyType     = FMovieSceneAnimTypeID;
	using StorageType = IMovieScenePreAnimatedGlobalTokenPtr;

	static void RestorePreAnimatedValue(FMovieSceneAnimTypeID, IMovieScenePreAnimatedGlobalTokenPtr& Token, const FRestoreStateParams& Params)
	{
		Token->RestoreState(Params);
	}
};

struct MOVIESCENE_API FAnimTypePreAnimatedStateMasterStorage : TPreAnimatedStateStorage<FPreAnimatedMasterTokenTraits>
{
	static TAutoRegisterPreAnimatedStorageID<FAnimTypePreAnimatedStateMasterStorage> StorageID;

	FPreAnimatedStateEntry MakeEntry(FMovieSceneAnimTypeID AnimTypeID);

public:

	FPreAnimatedStorageID GetStorageType() const override { return StorageID; }
	void Initialize(FPreAnimatedStorageID InStorageID, FPreAnimatedStateExtension* ParentExtension) override;
};



} // namespace MovieScene
} // namespace UE






