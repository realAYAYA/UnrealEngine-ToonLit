// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedObjectGroupManager.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"

namespace UE
{
namespace MovieScene
{

TAutoRegisterPreAnimatedStorageID<FPreAnimatedObjectGroupManager> FPreAnimatedObjectGroupManager::GroupManagerID;

void FPreAnimatedObjectGroupManager::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	TMap<FObjectKey, FPreAnimatedStorageGroupHandle> OldStorageGroupsByKey = MoveTemp(StorageGroupsByKey);
	StorageGroupsByKey.Reset();
	StorageGroupsByKey.Reserve(OldStorageGroupsByKey.Num());

	for (auto It = OldStorageGroupsByKey.CreateIterator(); It; ++It)
	{
		FPreAnimatedStorageGroupHandle GroupHandle = It.Value();

		UObject* Object = It.Key().ResolveObjectPtrEvenIfPendingKill();
		if (UObject* ReplacedObject = ReplacementMap.FindRef(Object))
		{
			FObjectKey NewKey(ReplacedObject);

			StorageGroupsByKey.Add(NewKey, GroupHandle);
			// This will overwrite the existing entry
			StorageGroupsToKey.Add(GroupHandle, NewKey);

			Extension->ReplaceObjectForGroup(GroupHandle, It.Key(), NewKey);
		}
		else
		{
			StorageGroupsByKey.Add(It.Key(), It.Value());
		}
	}
}

void FPreAnimatedObjectGroupManager::GetGroupsByClass(UClass* GeneratedClass, TArray<FPreAnimatedStorageGroupHandle>& OutGroupHandles)
{
	for (auto It = StorageGroupsByKey.CreateConstIterator(); It; ++It)
	{
		UObject* Object = It.Key().ResolveObjectPtrEvenIfPendingKill();
		if (Object && Object->IsA(GeneratedClass))
		{
			OutGroupHandles.Add(It.Value());
		}
	}
}

} // namespace MovieScene
} // namespace UE
