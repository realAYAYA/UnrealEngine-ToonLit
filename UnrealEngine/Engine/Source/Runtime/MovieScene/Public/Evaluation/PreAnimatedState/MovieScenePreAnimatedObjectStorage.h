// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Evaluation/PreAnimatedState/MovieSceneRestoreStateParams.h"
#include "Evaluation/PreAnimatedState/IMovieScenePreAnimatedStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedObjectGroupManager.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedEntityCaptureSource.h"
#include "EntitySystem/BuiltInComponentTypes.h"


namespace UE
{
namespace MovieScene
{



// struct FPreAnimatedStateStorageObjectTraits
// {
// 	using KeyType     = FObjectKey;
// 	using StorageType = IMovieScenePreAnimatedTokenPtr;
// 	static void CachePreAnimatedValue(const FObjectKey& Object, StorageType& OutCachedValue);
// 	static void RestorePreAnimatedValue(const FObjectKey& Object, StorageType& InOutCachedValue, const FRestoreStateParams& Params);
// };

template<typename ObjectTraits>
struct TPreAnimatedStateStorage_ObjectTraits
	: TPreAnimatedStateStorage<ObjectTraits>
	, IPreAnimatedObjectEntityStorage
{
	using KeyType = typename ObjectTraits::KeyType;
	using StorageType = typename ObjectTraits::StorageType;

	TPreAnimatedStateStorage_ObjectTraits()
	{}

public:

	IPreAnimatedObjectEntityStorage* AsObjectStorage() override
	{
		return this;
	}

	void BeginTrackingEntities(const FPreAnimatedTrackerParams& Params, TRead<FMovieSceneEntityID> EntityIDs, TRead<FRootInstanceHandle> InstanceHandles, TRead<UObject*> BoundObjects) override
	{
		const int32 Num = Params.Num;
		const bool  bWantsRestore = Params.bWantsRestoreState;

		if (!this->ParentExtension->IsCapturingGlobalState() && !bWantsRestore)
		{
			return;
		}

		FPreAnimatedEntityCaptureSource* EntityMetaData = this->ParentExtension->GetOrCreateEntityMetaData();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			UObject* BoundObject = BoundObjects[Index];
			FObjectKey Key{ BoundObject };

			FPreAnimatedStorageGroupHandle GroupHandle  = this->Traits.MakeGroup(BoundObject);
			FPreAnimatedStorageIndex       StorageIndex = this->GetOrCreateStorageIndex(Key);

			FPreAnimatedStateEntry Entry{ GroupHandle, FPreAnimatedStateCachedValueHandle{ this->StorageID, StorageIndex } };
			EntityMetaData->BeginTrackingEntity(Entry, EntityIDs[Index], InstanceHandles[Index], bWantsRestore);
		}
	}

	void CachePreAnimatedValues(const FCachePreAnimatedValueParams& Params, TArrayView<UObject* const> BoundObjects) override
	{
		for (UObject* BoundObject : BoundObjects)
		{
			CachePreAnimatedValue(Params, BoundObject);
		}
	}

	void CachePreAnimatedValue(const FCachePreAnimatedValueParams& Params, UObject* BoundObject)
	{
		FObjectKey Key{ BoundObject };

		FPreAnimatedStorageIndex       StorageIndex = this->GetOrCreateStorageIndex(Key);
		FPreAnimatedStorageGroupHandle GroupHandle  = this->Traits.MakeGroup(BoundObject);
		FPreAnimatedStateEntry         Entry        = FPreAnimatedStateEntry{ GroupHandle, FPreAnimatedStateCachedValueHandle{ this->StorageID, StorageIndex } };

		if (this->ParentExtension->IsCapturingGlobalState())
		{
			this->ParentExtension->EnsureMetaData(Entry);
		}
		else if (!this->ParentExtension->MetaDataExists(Entry))
		{
			return;
		}

		CachePreAnimatedValue(Params, Entry, BoundObject);
	}

	void CachePreAnimatedValue(const FCachePreAnimatedValueParams& Params, const FPreAnimatedStateEntry& Entry, UObject* BoundObject)
	{
		FPreAnimatedStorageIndex StorageIndex = Entry.ValueHandle.StorageIndex;

		EPreAnimatedStorageRequirement StorageRequirement = this->ParentExtension->GetStorageRequirement(Entry);
		if (!this->IsStorageRequirementSatisfied(StorageIndex, StorageRequirement))
		{
			StorageType NewValue;
			ObjectTraits::CachePreAnimatedValue(BoundObject, NewValue);

			this->AssignPreAnimatedValue(StorageIndex, StorageRequirement, MoveTemp(NewValue));
		}

		if (Params.bForcePersist)
		{
			this->ForciblyPersistStorage(StorageIndex);
		}
	}
};






} // namespace MovieScene
} // namespace UE






