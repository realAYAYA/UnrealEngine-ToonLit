// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/UnrealType.h"

class UObject;

/**
 * Interface for an object that can handle live-editing features of the camera assets.
 */
class IGameplayCamerasLiveEditManager : public TSharedFromThis<IGameplayCamerasLiveEditManager>
{
public:

	virtual ~IGameplayCamerasLiveEditManager() {}

	/**
	 * Register a new set of instantiated objects.
	 *
	 * @param InstantiatedObjects  A mapping between a source object and an instantiated object.
	 */
	virtual void RegisterInstantiatedObjects(const TMap<UObject*, UObject*> InstantiatedObjects) = 0;

	/**
	 * Request that a property change on the given source object should be replicated on any
	 * known related instantied objects.
	 */
	virtual void ForwardPropertyChange(const UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent) = 0;
};

