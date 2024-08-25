// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTabSpawner.h"

class FAvaSceneSettingsTabSpawner : public FAvaTabSpawner
{
public:
	static FName GetTabID();

	explicit FAvaSceneSettingsTabSpawner(const TSharedRef<IAvaEditor>& InEditor);

	//~ Begin IAvaTabSpawner
	virtual TSharedRef<SWidget> CreateTabBody() override;
	//~ End IAvaTabSpawner
};
