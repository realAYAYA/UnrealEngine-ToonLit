// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectMixerEditorSettings.generated.h"

UCLASS(config = ObjectMixer)
class OBJECTMIXEREDITOR_API UObjectMixerEditorSettings : public UObject
{
	GENERATED_BODY()
public:
	
	UObjectMixerEditorSettings(const FObjectInitializer& ObjectInitializer)
	{}
	
	/**
	 * If enabled, clicking an item in the mixer list will also select the item in the Scene Outliner.
	 * Alt + Click to select items in mixer without selecting the item in the Scene outliner.
	 * If disabled, selections will not sync unless Alt is held. Effectively, this is the opposite behavior.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Object Mixer")
	bool bSyncSelection = true;

	/**
	 * If false, a new object will be created every time the filter object is accessed.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Object Mixer")
	bool bExpandTreeViewItemsByDefault = true;
};
