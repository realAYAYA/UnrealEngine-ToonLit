// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedMasterTokenStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"

namespace UE
{
namespace MovieScene
{

TAutoRegisterPreAnimatedStorageID<FAnimTypePreAnimatedStateMasterStorage> FAnimTypePreAnimatedStateMasterStorage::StorageID;

void FAnimTypePreAnimatedStateMasterStorage::Initialize(FPreAnimatedStorageID InStorageID, FPreAnimatedStateExtension* InParentExtension)
{
	TPreAnimatedStateStorage<FPreAnimatedMasterTokenTraits>::Initialize(InStorageID, InParentExtension);
}

FPreAnimatedStateEntry FAnimTypePreAnimatedStateMasterStorage::MakeEntry(FMovieSceneAnimTypeID AnimTypeID)
{
	// Find the storage index for the specific anim-type and object we're animating
	FPreAnimatedStorageIndex StorageIndex = GetOrCreateStorageIndex(AnimTypeID);
	return FPreAnimatedStateEntry{ FPreAnimatedStorageGroupHandle(), FPreAnimatedStateCachedValueHandle{ StorageID, StorageIndex } };
}



} // namespace MovieScene
} // namespace UE






