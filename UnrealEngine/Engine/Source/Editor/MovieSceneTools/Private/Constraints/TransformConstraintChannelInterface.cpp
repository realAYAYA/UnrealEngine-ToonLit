// Copyright Epic Games, Inc. All Rights Reserved.

#include "Constraints/TransformConstraintChannelInterface.h"
#include "Sections/MovieSceneConstrainedSection.h"
#include "ConstraintChannel.h"

// IMovieSceneConstrainedSection* ITransformConstraintChannelInterface::GetHandleConstraintSection(const UTransformableHandle* InHandle, const TSharedPtr<ISequencer>& InSequencer)
// {
// 	return Cast<IMovieSceneConstrainedSection>(GetHandleSection(InHandle, InSequencer));
// }
//we changed this so that it will always return true
bool ITransformConstraintChannelInterface::CanAddKey(FMovieSceneConstraintChannel& ActiveChannel, const FFrameNumber& InTime, bool& ActiveValue)
{
	TMovieSceneChannelData<bool> ChannelData = ActiveChannel.GetData();
	const TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
	if (Times.IsEmpty())
	{
		ActiveValue = true;
		return true;
	}
	ActiveChannel.Evaluate(InTime, ActiveValue);
	ActiveValue = !ActiveValue;
	const int32 Index = Times.Find(InTime);
	const int32 NextTimeIndex = Algo::UpperBound(Times, InTime);
	//have key at that time so need to delete it
	auto DeleteNextKey = [&ActiveChannel, ActiveValue, &Times, Index,NextTimeIndex]()
	{
		TMovieSceneChannelData<bool> ChannelData = ActiveChannel.GetData();
		//if has same value then delete the next
		if (NextTimeIndex != Index && Times.IsValidIndex(NextTimeIndex))
		{
			bool NextValue = false;
			ActiveChannel.Evaluate(Times[NextTimeIndex], NextValue);
			if (NextValue == ActiveValue)
			{
				//same value so delete
				ChannelData.RemoveKey(NextTimeIndex);
			}
		}
	};
	if (Index != INDEX_NONE)
	{
		DeleteNextKey();
		//delete key will add it later
		ChannelData.RemoveKey(Index);
		//if last key then we won't add any more keys
		if (Index == (Times.Num() - 1))
		{
			return false;
		}
	}
	//if has same value then delete the next
	else if (Times.IsValidIndex(NextTimeIndex))
	{
		DeleteNextKey();
	}
	
	return true;
}

FConstraintChannelInterfaceRegistry& FConstraintChannelInterfaceRegistry::Get()
{
	static FConstraintChannelInterfaceRegistry Singleton;
	return Singleton;
}

ITransformConstraintChannelInterface* FConstraintChannelInterfaceRegistry::FindConstraintChannelInterface(const UClass* InClass) const
{
	const TUniquePtr<ITransformConstraintChannelInterface>* Interface = HandleToInterfaceMap.Find(InClass);
	ensureMsgf(Interface, TEXT("No constraint channel interface found for class %s. Did you call RegisterConstraintChannelInterface<> for that class?"), *InClass->GetName());
	return Interface ? Interface->Get() : nullptr;
}