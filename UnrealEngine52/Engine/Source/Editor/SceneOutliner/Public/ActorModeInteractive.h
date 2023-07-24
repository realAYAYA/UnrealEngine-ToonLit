// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorMode.h"

class FActorModeInteractive : public FActorMode
{
public:
	FActorModeInteractive(const FActorModeParams& Params);
	virtual ~FActorModeInteractive();

	virtual bool IsInteractive() const override { return true; }
private:
	/* Events */

	void OnMapChange(uint32 MapFlags);
	void OnNewCurrentLevel();

	void OnLevelSelectionChanged(UObject* Obj);
	void OnActorLabelChanged(AActor* ChangedActor);
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);
	void OnLevelActorRequestsRename(const AActor* Actor);
	void OnPostLoadMapWithWorld(UWorld* World);
};
