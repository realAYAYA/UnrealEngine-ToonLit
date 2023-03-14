// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectMixerEditorModule.h"

class LIGHTMIXER_API FLightMixerModule : public FObjectMixerEditorModule
{
public:

	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

	static FLightMixerModule& Get();
	
	static void OpenProjectSettings();
	
	//~ Begin FObjectMixerEditorModule overrides
	virtual void Initialize() override;
	virtual FName GetModuleName() override;
	virtual void SetupMenuItemVariables() override;
	virtual FName GetTabSpawnerId() override;
	virtual void RegisterSettings() const override;
	virtual void UnregisterSettings() const override;
	//~ End FObjectMixerEditorModule overrides
};
