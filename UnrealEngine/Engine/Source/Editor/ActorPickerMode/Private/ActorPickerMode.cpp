// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorPickerMode.h"
#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "EditorModeActorPicker.h" 
#include "Framework/Application/SlateApplication.h"

IMPLEMENT_MODULE( FActorPickerModeModule, ActorPickerMode );

void FActorPickerModeModule::StartupModule()
{
	FEditorModeRegistry::Get().RegisterMode<FEdModeActorPicker>(FBuiltinEditorModes::EM_ActorPicker);

	if (FSlateApplication::IsInitialized())
	{
		OnApplicationDeactivatedHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().Add(TDelegate<void(const bool)>::CreateRaw(this, &FActorPickerModeModule::OnApplicationDeactivated));
	}
}

void FActorPickerModeModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(OnApplicationDeactivatedHandle);
		OnApplicationDeactivatedHandle.Reset();
	}

	FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_ActorPicker);
}

void FActorPickerModeModule::BeginActorPickingMode(FOnGetAllowedClasses InOnGetAllowedClasses, FOnShouldFilterActor InOnShouldFilterActor, FOnActorSelected InOnActorSelected) const
{
	if(!GLevelEditorModeToolsIsValid())
	{
		return;
	}

	// Activate the mode
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_ActorPicker);

	// Set the required delegates
	FEdModeActorPicker* Mode = GLevelEditorModeTools().GetActiveModeTyped<FEdModeActorPicker>(FBuiltinEditorModes::EM_ActorPicker);
	if (ensure(Mode))
	{
		Mode->OnActorSelected = InOnActorSelected;
		Mode->OnGetAllowedClasses = InOnGetAllowedClasses;
		Mode->OnShouldFilterActor = InOnShouldFilterActor;
	}
}

void FActorPickerModeModule::OnApplicationDeactivated(const bool IsActive) const
{
	if (!IsActive) 
	{ 
		EndActorPickingMode();
	}
}

void FActorPickerModeModule::EndActorPickingMode() const
{
	if (IsInActorPickingMode() && GLevelEditorModeToolsIsValid())
	{
		GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_ActorPicker);
	}
}

bool FActorPickerModeModule::IsInActorPickingMode() const
{
	if(GLevelEditorModeToolsIsValid())
	{
		return GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_ActorPicker);
	}
	return false;
}
