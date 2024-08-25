// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncAvaBridgeEditor.h"
#include "Modules/ModuleManager.h"
#include "StormSyncAvaBridgeEditorLog.h"
#include "StormSyncAvaRundownExtender.h"

DEFINE_LOG_CATEGORY(LogStormSyncAvaBridgeEditor);

void FStormSyncAvaBridgeEditorModule::StartupModule()
{
	RundownExtender = MakeShared<FStormSyncAvaRundownExtender>();
}

void FStormSyncAvaBridgeEditorModule::ShutdownModule()
{
	RundownExtender.Reset();
}

IMPLEMENT_MODULE(FStormSyncAvaBridgeEditorModule, StormSyncAvaBridgeEditor)
