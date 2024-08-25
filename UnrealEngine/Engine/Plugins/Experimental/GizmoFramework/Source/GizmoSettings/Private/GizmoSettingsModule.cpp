// Copyright Epic Games, Inc. All Rights Reserved.

#include "GizmoSettingsModule.h"
#include "Modules/ModuleManager.h"

#include "GizmoSettings.h"
#include "EditorInteractiveGizmoManager.h"

void FGizmoSettingsModule::StartupModule()
{
	UGizmoSettings::OnSettingsChange.AddRaw(this, &FGizmoSettingsModule::UpdateGizmos);
	UpdateGizmos(GetDefault<UGizmoSettings>());
}

void FGizmoSettingsModule::ShutdownModule()
{
	UGizmoSettings::OnSettingsChange.RemoveAll(this);
}

void FGizmoSettingsModule::UpdateGizmos(const UGizmoSettings* InSettings)
{
	if (InSettings)
	{
		UEditorInteractiveGizmoManager::SetUsesNewTRSGizmos(InSettings->bEnableNewGizmos);
		UEditorInteractiveGizmoManager::SetGizmosParameters(InSettings->GizmoParameters);
	}
}


IMPLEMENT_MODULE(FGizmoSettingsModule, GizmoSettings)
