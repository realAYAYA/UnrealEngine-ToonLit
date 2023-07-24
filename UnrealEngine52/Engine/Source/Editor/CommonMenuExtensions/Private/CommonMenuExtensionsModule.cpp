// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonMenuExtensionsModule.h"
#include "BufferVisualizationMenuCommands.h"
#include "NaniteVisualizationMenuCommands.h"
#include "LumenVisualizationMenuCommands.h"
#include "StrataVisualizationMenuCommands.h"
#include "GroomVisualizationMenuCommands.h"
#include "VirtualShadowMapVisualizationMenuCommands.h"
#include "ShowFlagMenuCommands.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FCommonMenuExtensionsModule, CommonMenuExtensions);

void FCommonMenuExtensionsModule::StartupModule()
{
	FBufferVisualizationMenuCommands::Register();
	FNaniteVisualizationMenuCommands::Register();
	FLumenVisualizationMenuCommands::Register();
	FStrataVisualizationMenuCommands::Register();
	FGroomVisualizationMenuCommands::Register();
	FVirtualShadowMapVisualizationMenuCommands::Register();
	FShowFlagMenuCommands::Register();
}

void FCommonMenuExtensionsModule::ShutdownModule()
{
	FShowFlagMenuCommands::Unregister();
	FVirtualShadowMapVisualizationMenuCommands::Unregister();
	FNaniteVisualizationMenuCommands::Unregister();
	FGroomVisualizationMenuCommands::Unregister();
	FStrataVisualizationMenuCommands::Unregister();
	FLumenVisualizationMenuCommands::Unregister();
	FBufferVisualizationMenuCommands::Unregister();
}