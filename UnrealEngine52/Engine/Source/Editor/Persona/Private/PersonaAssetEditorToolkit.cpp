// Copyright Epic Games, Inc. All Rights Reserved.

#include "PersonaAssetEditorToolkit.h"

#include "PersonaModule.h"
#include "AssetEditorModeManager.h"
#include "Modules/ModuleManager.h"
#include "IPersonaEditorModeManager.h"

void FPersonaAssetEditorToolkit::CreateEditorModeManager()
{
	EditorModeManager = MakeShareable(FModuleManager::LoadModuleChecked<FPersonaModule>("Persona").CreatePersonaEditorModeManager());
}
