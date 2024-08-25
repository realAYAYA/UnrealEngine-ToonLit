// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AsyncCompilationHelpers.h"
#include "Containers/Map.h"
#include "Containers/Array.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

class FTextFormat;

struct FChaosVDWailingObjectListCVars
{
	static int32 NumOfGTTaskToProcessPerTick;
	static FAutoConsoleVariableRef CVarChaosVDGeometryToProcessPerTick;
};

/** Object that handles objects waiting for geometry to be ready to perform a desired operation
 * The operation has to run on the Game Thread, and there is a limit on how many operations will be processed per tick
 */
template<typename TObjectToProcess>
class FObjectsWaitingGeometryList
{

public:
	 FObjectsWaitingGeometryList(const TFunction<bool(uint32, TObjectToProcess)>& InObjectProcessorCallback, const FTextFormat& InProgressNotificationNameFormat, const TFunction<bool(uint32)>& InShouldProcessObjectsOverride) :
		  ObjectProcessorCallback(InObjectProcessorCallback)
		, ShouldProcessObjectsForKeyOverride(InShouldProcessObjectsOverride)
		, AsyncProgressNotification(InProgressNotificationNameFormat)
	{
	}

	/** Adds an object to process when the provided geometry key is ready */
	void AddObject(uint32 GeometryKey, TObjectToProcess ObjectToProcess);

	/** Removed an object to process */
	void RemoveObject(uint32 GeometryKey, TObjectToProcess& OutToProcess);

	/** Evaluates the current geometry available, and process any objects on the waiting list for it */
	bool ProcessWaitingObjects(int32& OutCurrentElementsProcessed);

private:

	bool ShouldProcessObjectsForKey(uint32 GeometryKey) const
	{
		if (ShouldProcessObjectsForKeyOverride)
		{
			return ShouldProcessObjectsForKeyOverride(GeometryKey);
		}
		return true;
	}

	TFunction<bool(uint32, TObjectToProcess)> ObjectProcessorCallback;

	TFunction<bool(uint32)> ShouldProcessObjectsForKeyOverride;

	TMap<uint32, TArray<TObjectToProcess>> WaitingObjectsByGeometryKey;

	FAsyncCompilationNotification AsyncProgressNotification;

	int32 QueuedObjectsToProcessNum = 0;
};

template <typename TObjectToProcess>
void FObjectsWaitingGeometryList<TObjectToProcess>::AddObject(uint32 GeometryKey, TObjectToProcess ObjectToProcess)
{
	check(IsInGameThread());

	if (TArray<TObjectToProcess>* ObjectsInQueueForKey = WaitingObjectsByGeometryKey.Find(GeometryKey))
	{
		ObjectsInQueueForKey->Add(ObjectToProcess);
	}
	else
	{
		WaitingObjectsByGeometryKey.Add(GeometryKey, {ObjectToProcess});
	}

	++QueuedObjectsToProcessNum;
}

template <typename TObjectToProcess>
void FObjectsWaitingGeometryList<TObjectToProcess>::RemoveObject(uint32 GeometryKey, TObjectToProcess& OutToProcess)
{
	check(IsInGameThread());

	if (TArray<TObjectToProcess>* QueueObjectsForKey = WaitingObjectsByGeometryKey.Find(GeometryKey))
	{
		TArray<TObjectToProcess>& QueueObjectsForKeyRef = *QueueObjectsForKey;
		for(typename TArray<TObjectToProcess>::TIterator ObjectToRemoveIterator = QueueObjectsForKeyRef.CreateIterator();  ObjectToRemoveIterator; ++ObjectToRemoveIterator)
		{
			TObjectToProcess& ObjectToRemove = *ObjectToRemoveIterator;

			if (ObjectToRemove == OutToProcess)
			{
				ObjectToRemoveIterator.RemoveCurrent();

				--QueuedObjectsToProcessNum;
			}
		}		
	}
}

template <typename TObjectToProcess>
bool FObjectsWaitingGeometryList<TObjectToProcess>::ProcessWaitingObjects(int32& OutCurrentElementsProcessed)
{
	AsyncProgressNotification.Update(QueuedObjectsToProcessNum);

	bool bCanContinueProcessing = true;
	for (typename TMap<uint32, TArray<TObjectToProcess>>::TIterator QueuedObjectRemoveIterator = WaitingObjectsByGeometryKey.CreateIterator(); QueuedObjectRemoveIterator; ++QueuedObjectRemoveIterator)
	{
		const uint32 GeometryKey = QueuedObjectRemoveIterator.Key();

		if (ShouldProcessObjectsForKey(GeometryKey))
		{
			for (typename TArray<TObjectToProcess>::TIterator ObjectRemoveIterator = QueuedObjectRemoveIterator.Value().CreateIterator(); ObjectRemoveIterator; ++ObjectRemoveIterator)
			{
				if (ObjectProcessorCallback && ObjectProcessorCallback(GeometryKey, *ObjectRemoveIterator))
				{
					OutCurrentElementsProcessed++;
					--QueuedObjectsToProcessNum;
					bCanContinueProcessing = OutCurrentElementsProcessed < FChaosVDWailingObjectListCVars::NumOfGTTaskToProcessPerTick;
					ObjectRemoveIterator.RemoveCurrent();
				}
			}
		}

		if (!bCanContinueProcessing)
		{
			break;
		}	

		if (QueuedObjectRemoveIterator.Value().IsEmpty())
		{
			QueuedObjectRemoveIterator.RemoveCurrent();
		}

		if (!bCanContinueProcessing)
		{
			break;
		}
	}

	return bCanContinueProcessing;
}
