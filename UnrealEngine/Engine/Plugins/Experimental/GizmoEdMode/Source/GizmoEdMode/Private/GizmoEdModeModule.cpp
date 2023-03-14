// Copyright Epic Games, Inc. All Rights Reserved.

#include "GizmoEdModeModule.h"
#include "GizmoEdMode.h"
#include "EditorModeRegistry.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FGizmoEdModeModule"

void FGizmoEdModeModule::OnPostEngineInit()
{
}

void FGizmoEdModeModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FGizmoEdModeModule::OnPostEngineInit);
}

void FGizmoEdModeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGizmoEdModeModule, GizmoEdMode);
