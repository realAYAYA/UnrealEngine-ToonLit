// Copyright Epic Games, Inc. All Rights Reserved.

#include "Constraints/TransformConstraintChannelInterface.h"
#include "Sections/MovieSceneConstrainedSection.h"
#include "ConstraintChannel.h"

// IMovieSceneConstrainedSection* ITransformConstraintChannelInterface::GetHandleConstraintSection(const UTransformableHandle* InHandle, const TSharedPtr<ISequencer>& InSequencer)
// {
// 	return Cast<IMovieSceneConstrainedSection>(GetHandleSection(InHandle, InSequencer));
// }

bool ITransformConstraintChannelInterface::CanAddKey(const FMovieSceneConstraintChannel& ActiveChannel, const FFrameNumber& InTime, bool& ActiveValue)
{
	const TMovieSceneChannelData<const bool> ChannelData = ActiveChannel.GetData();
	const TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
	if (Times.IsEmpty())
	{
		ActiveValue = true;
		return true;
	}

	const TArrayView<const bool> Values = ChannelData.GetValues();
	if (InTime < Times[0])
	{
		if (!Values[0])
		{
			ActiveValue = true;
			return true;
		}
		return false;
	}
	
	if (InTime > Times.Last())
	{
		ActiveValue = !Values.Last();
		return true;
	}

	return false;
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