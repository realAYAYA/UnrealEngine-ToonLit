// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityEditorModule.h"
#include "MassEditorStyle.h"
#include "Modules/ModuleManager.h"


#define LOCTEXT_NAMESPACE "MassEntityEditor"

IMPLEMENT_MODULE(FMassEntityEditorModule, MassEntityEditor)

void FMassEntityEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FMassEntityEditorStyle::Initialize();
}

void FMassEntityEditorModule::ShutdownModule()
{
	ProcessorClassCache.Reset();
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	FMassEntityEditorStyle::Shutdown();
}
#undef LOCTEXT_NAMESPACE
