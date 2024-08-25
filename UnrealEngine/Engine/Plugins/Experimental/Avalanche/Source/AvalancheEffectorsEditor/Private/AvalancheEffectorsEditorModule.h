// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointerFwd.h"

class FComponentVisualizer;
class IAvalancheInteractiveToolsModule;

class FAvalancheEffectorsEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	void PostEngineInit();

	void RegisterTools(IAvalancheInteractiveToolsModule* InModule);

	void RegisterComponentVisualizers();

	TArray<TSharedPtr<FComponentVisualizer>> Visualizers;
};
