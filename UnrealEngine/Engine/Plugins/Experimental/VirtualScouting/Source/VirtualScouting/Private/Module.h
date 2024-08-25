// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


class FVirtualScoutingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Register the settings class with the settings module **/
	void RegisterSettings() const;

	/** Unregister the settings class with the settings module **/
	void UnregisterSettings() const;
	
	/** TODO Handler for when Virtual Scouting settings are changed. */
	bool OnSettingsModified();
};
