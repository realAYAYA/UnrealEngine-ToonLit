// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IGameplayCamerasLiveEditManager.h"

#include "CoreTypes.h"
#include "UObject/WeakObjectPtr.h"

class FGameplayCamerasLiveEditManager : public IGameplayCamerasLiveEditManager
{
public:

	FGameplayCamerasLiveEditManager();

	/** Clean-up any invalid entries in the map of known instantiated objects. */
	void CleanUp();

public:

	// IGameplayCamerasLiveEditManager interface
	virtual void RegisterInstantiatedObjects(const TMap<UObject*, UObject*> InstantiatedObjects) override;
	virtual void ForwardPropertyChange(const UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent) override;

private:

	struct FInstantiationInfo
	{
		TArray<TWeakObjectPtr<>> InstantiatedObjects;
	};
	TMap<TWeakObjectPtr<UObject>, FInstantiationInfo> Instantiations;
};

