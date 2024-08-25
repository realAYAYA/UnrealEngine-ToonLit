// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorToolkit.h"

class FAvaOutlinerItem;
class IAssetTypeActions;
class IAvaEditor;

DECLARE_LOG_CATEGORY_EXTERN(AvaLog, Log, All);

/**
 * Main Motion Design Editor Module
 */
class FAvaEditorModule : public IModuleInterface
{
public:
	//~ Begin IAvaEditorModule
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IAvaEditorModule

	static FSlateIcon GetOutlinerShapeActorIcon(TSharedPtr<const FAvaOutlinerItem> InItem);
	
private:
	/** Motion Design Level Editor */
	TSharedPtr<IAvaEditor> AvaLevelEditor;

	void CreateAvaLevelEditor();

	void PostEngineInit();
	void PreExit();

	void RegisterAssetTools();
	void RegisterPropertyEditorCategories();

	void RegisterCustomLayouts();
	void UnregisterCustomLayouts();

	void RegisterLevelTemplates();
};
