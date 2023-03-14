// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataChartsEditor.h"
#include "DataChartsPlacement.h"
#include "DataChartsStyle.h"

#define LOCTEXT_NAMESPACE "FDataChartsEditorModule"

void FDataChartsEditorModule::StartupModule()
{
	FDataChartsStyle::Initialize();
	FDataChartsPlacement::RegisterPlacement();
}

void FDataChartsEditorModule::ShutdownModule()
{
	FDataChartsStyle::Shutdown();
	FDataChartsPlacement::UnregisterPlacement();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDataChartsEditorModule, DataChartsEditor)