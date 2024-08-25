// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedObjectStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedObjectGroupManager.h"

namespace UE
{
namespace MovieScene
{

void FBoundObjectPreAnimatedStateTraits::Initialize(FPreAnimatedStorageID InStorageID, FPreAnimatedStateExtension* InParentExtension)
{
	ObjectGroupManager = InParentExtension->GetOrCreateGroupManager<FPreAnimatedObjectGroupManager>();
}

FPreAnimatedStorageGroupHandle FBoundObjectPreAnimatedStateTraits::FindGroupImpl(UObject* BoundObject)
{
	return ObjectGroupManager->FindGroupForKey(BoundObject);
}

FPreAnimatedStorageGroupHandle FBoundObjectPreAnimatedStateTraits::FindGroupImpl(const FObjectComponent& BoundObject)
{
	return ObjectGroupManager->FindGroupForKey(BoundObject.GetObject());
}

FPreAnimatedStorageGroupHandle FBoundObjectPreAnimatedStateTraits::MakeGroupImpl(UObject* BoundObject)
{
	return ObjectGroupManager->MakeGroupForKey(BoundObject);
}

FPreAnimatedStorageGroupHandle FBoundObjectPreAnimatedStateTraits::MakeGroupImpl(const FObjectComponent& BoundObject)
{
	return ObjectGroupManager->MakeGroupForKey(BoundObject.GetObject());
}

} // namespace MovieScene
} // namespace UE

