// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

AUDIOGAMEPLAYVOLUMEEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(AudioGameplayVolumeEditor, Log, All);

class FAudioGameplayVolumeEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:

	void HandleCustomPropertyLayouts(bool bRegisterLayouts);
};
