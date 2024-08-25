// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagHandleContainer.h"
#include "UObject/Object.h"
#include "AvaSceneState.generated.h"

class UAvaSceneSettings;

/** Object providing State information of the Scene */
UCLASS(MinimalAPI)
class UAvaSceneState : public UObject
{
	GENERATED_BODY()

public:
	void SetSceneSettings(UAvaSceneSettings* InSceneSettings);

	AVALANCHE_API bool AddTagAttribute(const FAvaTagHandle& InTagHandle);

	AVALANCHE_API bool RemoveTagAttribute(const FAvaTagHandle& InTagHandle);

	AVALANCHE_API bool ContainsTagAttribute(const FAvaTagHandle& InTagHandle) const;

private:
	/** Weak pointer to the same scene's Settings. Used to query for default tag attributes of the scene */
	TWeakObjectPtr<const UAvaSceneSettings> SceneSettingsWeak;

	/** Active Tag Attributes that were added to the scene, separate from Scene Settings */
	TSet<FAvaTag> ActiveTagAttributes;
};
