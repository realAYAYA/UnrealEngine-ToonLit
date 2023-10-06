// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCinePrestreamingEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	void StartupModule() override;
	void ShutdownModule() override;
	
private:
	void RegisterTrackEditors();
	void UnregisterTrackEditors();
	
	FDelegateHandle CinePrestreamingCreateEditorHandle;
};
