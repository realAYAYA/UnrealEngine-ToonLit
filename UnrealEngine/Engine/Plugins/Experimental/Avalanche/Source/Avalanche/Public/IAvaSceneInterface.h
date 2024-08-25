// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAvaSceneInterface.generated.h"

class IAvaSequencePlaybackObject;
class IAvaSequenceProvider;
class UAvaSceneSettings;
class UAvaSceneState;
class ULevel;
class URemoteControlPreset;
struct FAvaSceneTree;

/**
 * Interface for the driver class of an Motion Design Scene
 * This interface's purpose is to provide access to scene data and to core interfaces in the Scene
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAvaSceneInterface : public UInterface
{
	GENERATED_BODY()
};

class IAvaSceneInterface
{
	GENERATED_BODY()

public:
	virtual ULevel* GetSceneLevel() const = 0;

	virtual UAvaSceneSettings* GetSceneSettings() const = 0;

	virtual UAvaSceneState* GetSceneState() const = 0;

	virtual const FAvaSceneTree& GetSceneTree() const = 0;

	virtual FAvaSceneTree& GetSceneTree() = 0;

	virtual IAvaSequenceProvider* GetSequenceProvider() = 0;

	virtual const IAvaSequenceProvider* GetSequenceProvider() const = 0;

	virtual IAvaSequencePlaybackObject* GetPlaybackObject() const = 0;

	virtual URemoteControlPreset* GetRemoteControlPreset() const = 0;
};
