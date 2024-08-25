// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class FComponentVisualizer;
class FName;
class IAvalancheInteractiveToolsModule;
class UEdMode;
struct FAvaInteractiveToolsToolParameters;

class FAvaTextEditorModule : public IModuleInterface
{
public:

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	TArray<TSharedPtr<FComponentVisualizer>> Visualizers;

	void PostEngineInit();

	void RegisterTools(IAvalancheInteractiveToolsModule* InModule);

	void RegisterComponentVisualizers();

	void RegisterCustomLayouts();
	void UnregisterCustomLayouts();

	void RegisterDynamicMaterialPropertyGenerator();
};
