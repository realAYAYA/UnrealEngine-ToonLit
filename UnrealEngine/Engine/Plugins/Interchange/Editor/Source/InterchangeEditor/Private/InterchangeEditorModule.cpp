// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeEditorModule.h"

#include "InterchangeEditorLog.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"



#define LOCTEXT_NAMESPACE "InterchangeEditorModule"

DEFINE_LOG_CATEGORY(LogInterchangeEditor);

FInterchangeEditorModule& FInterchangeEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked< FInterchangeEditorModule >(INTERCHANGEEDITOR_MODULE_NAME);
}

bool FInterchangeEditorModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(INTERCHANGEEDITOR_MODULE_NAME);
}

void FInterchangeEditorModule::StartupModule()
{
}

IMPLEMENT_MODULE(FInterchangeEditorModule, InterchangeEditor);

#undef LOCTEXT_NAMESPACE // "InterchangeEditorModule"

