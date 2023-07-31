// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedEntityCaptureSource.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"
#include "Evaluation/PreAnimatedState/MovieSceneRestoreStateParams.h"
#include "Algo/Find.h"

namespace UE
{
namespace MovieScene
{


FPreAnimatedEntityCaptureSource::FPreAnimatedEntityCaptureSource(FPreAnimatedStateExtension* InOwner)
	: Owner(InOwner)
{}

void FPreAnimatedEntityCaptureSource::Reset()
{
	KeyToMetaData.Empty();
}

void FPreAnimatedEntityCaptureSource::BeginTrackingEntity(const FPreAnimatedStateEntry& Entry, FMovieSceneEntityID EntityID, FRootInstanceHandle InstanceHandle, bool bWantsRestoreState)
{
	FPreAnimatedStateMetaDataArray& Array = KeyToMetaData.FindOrAdd(EntityID);
	if (FPreAnimatedStateMetaData* Existing = Algo::FindBy(Array, Entry, &FPreAnimatedStateMetaData::Entry))
	{
		if (Existing->bWantsRestoreState != bWantsRestoreState)
		{
			Existing->bWantsRestoreState = bWantsRestoreState;
			Owner->UpdateMetaData(*Existing);
		}
	}
	else
	{
		FPreAnimatedStateMetaData NewMetaData{ Entry, InstanceHandle, bWantsRestoreState };

		Array.Add(NewMetaData);
		Owner->AddMetaData(NewMetaData);
	}
}

void FPreAnimatedEntityCaptureSource::StopTrackingEntity(FMovieSceneEntityID EntityID, FPreAnimatedStorageID StorageID)
{
	FPreAnimatedStateMetaDataArray* Array = KeyToMetaData.Find(EntityID);
	if (Array)
	{
		for (int32 Index = Array->Num()-1; Index >= 0; --Index)
		{
			const FPreAnimatedStateMetaData& MetaData = (*Array)[Index];
			if (MetaData.Entry.ValueHandle.TypeID == StorageID)
			{
				Owner->RemoveMetaData(MetaData);
				Array->RemoveAt(Index, 1, false);
			}
		}

		if (Array->Num() == 0)
		{
			KeyToMetaData.Remove(EntityID);
		}
	}
}

void FPreAnimatedEntityCaptureSource::StopTrackingEntity(FMovieSceneEntityID EntityID)
{
	FPreAnimatedStateMetaDataArray* Array = KeyToMetaData.Find(EntityID);
	if (Array)
	{
		for (int32 Index = Array->Num()-1; Index >= 0; --Index)
		{
			const FPreAnimatedStateMetaData& MetaData = (*Array)[Index];
			Owner->RemoveMetaData(MetaData);
		}

		KeyToMetaData.Remove(EntityID);
	}
}

void FPreAnimatedEntityCaptureSource::GatherAndRemoveExpiredMetaData(const FRestoreStateParams& Params, TArray<FPreAnimatedStateMetaData>& OutExpiredMetaData)
{
	FInstanceHandle InstanceHandle = Params.TerminalInstanceHandle;
	if (!InstanceHandle.IsValid())
	{
		for (auto It = KeyToMetaData.CreateIterator(); It; ++It)
		{
			OutExpiredMetaData.Append(It.Value());
		}
		KeyToMetaData.Empty();
	}
	else for (auto It = KeyToMetaData.CreateIterator(); It; ++It)
	{
		FPreAnimatedStateMetaDataArray& Array = It.Value();
		for (int32 Index = Array.Num()-1; Index >= 0; --Index)
		{
			FPreAnimatedStateMetaData& MetaData = Array[Index];
			if (MetaData.RootInstanceHandle == InstanceHandle)
			{
				OutExpiredMetaData.Add(MetaData);
				Array.RemoveAt(Index, 1, false);
			}
		}

		if (Array.Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}

void FPreAnimatedEntityCaptureSource::GatherAndRemoveMetaDataForGroup(FPreAnimatedStorageGroupHandle Group, TArray<FPreAnimatedStateMetaData>& OutExpiredMetaData)
{
	for (auto It = KeyToMetaData.CreateIterator(); It; ++It)
	{
		FPreAnimatedStateMetaDataArray& Array = It.Value();
		for (int32 Index = Array.Num()-1; Index >= 0; --Index)
		{
			const FPreAnimatedStateMetaData& MetaData = Array[Index];
			if (MetaData.Entry.GroupHandle == Group)
			{
				OutExpiredMetaData.Add(MetaData);
				Array.RemoveAt(Index, 1, false);
			}
		}

		if (Array.Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}

bool FPreAnimatedEntityCaptureSource::ContainsInstanceHandle(FRootInstanceHandle RootInstanceHandle) const
{
	for (const TPair<FMovieSceneEntityID, FPreAnimatedStateMetaDataArray>& Pair : KeyToMetaData)
	{
		for (const FPreAnimatedStateMetaData& MetaData : Pair.Value)
		{
			if (MetaData.RootInstanceHandle == RootInstanceHandle)
			{
				return true;
			}
		}
	}
	return false;
}

} // namespace MovieScene
} // namespace UE
