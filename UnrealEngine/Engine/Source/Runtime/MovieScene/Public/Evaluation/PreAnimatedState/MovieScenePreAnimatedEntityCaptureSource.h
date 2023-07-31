// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreTypes.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateTypes.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.h"

namespace UE
{
namespace MovieScene
{

struct FPreAnimatedStateExtension;
struct FPreAnimatedStorageID;
struct FRestoreStateParams;
struct FRootInstanceHandle;

/**
 * Structure responsible for tracking contributions to pre-eanimated state entries that originate from ECS data (ie, from FMovieSceneEntityIDs)
 */
struct FPreAnimatedEntityCaptureSource
{
	FPreAnimatedEntityCaptureSource(FPreAnimatedStateExtension* InOwner);

	void Reset();

	MOVIESCENE_API void BeginTrackingEntity(const FPreAnimatedStateEntry& Entry, FMovieSceneEntityID EntityID, FRootInstanceHandle RootInstanceHandle, bool bWantsRestoreState);
	MOVIESCENE_API void StopTrackingEntity(FMovieSceneEntityID EntityID, FPreAnimatedStorageID StorageID);
	MOVIESCENE_API void StopTrackingEntity(FMovieSceneEntityID EntityID);

	bool ContainsInstanceHandle(FRootInstanceHandle RootInstanceHandle) const;
	void GatherAndRemoveExpiredMetaData(const FRestoreStateParams& Params, TArray<FPreAnimatedStateMetaData>& OutExpiredMetaData);
	void GatherAndRemoveMetaDataForGroup(FPreAnimatedStorageGroupHandle Group, TArray<FPreAnimatedStateMetaData>& OutExpiredMetaData);

private:

	TMap<FMovieSceneEntityID, FPreAnimatedStateMetaDataArray> KeyToMetaData;
	FPreAnimatedStateExtension* Owner;
};




} // namespace MovieScene
} // namespace UE
