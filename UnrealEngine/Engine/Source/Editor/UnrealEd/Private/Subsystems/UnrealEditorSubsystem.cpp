// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/UnrealEditorSubsystem.h"
#include "Editor.h"
#include "SLevelViewport.h"
#include "LevelEditor.h"
#include "Utils.h"
#include "GameFramework/Actor.h"
#include "EditorScriptingHelpers.h"

namespace InternalUnrealEditorSubsystemLibrary
{
	UWorld* GetEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
	}

	UWorld* GetGameWorld()
	{
		if (GEditor)
		{
			if (FWorldContext* WorldContext = GEditor->GetPIEWorldContext())
			{
				return WorldContext->World();
			}

			return nullptr;
		}

		return GWorld;
	}
}

bool UUnrealEditorSubsystem::GetLevelViewportCameraInfo(FVector& CameraLocation, FRotator& CameraRotation)
{
	bool RetVal = false;
	CameraLocation = FVector::ZeroVector;
	CameraRotation = FRotator::ZeroRotator;

	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->IsPerspective())
		{
			CameraLocation = LevelVC->GetViewLocation();
			CameraRotation = LevelVC->GetViewRotation();
			RetVal = true;

			break;
		}
	}

	return RetVal;
}

void UUnrealEditorSubsystem::SetLevelViewportCameraInfo(FVector CameraLocation, FRotator CameraRotation)
{
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->IsPerspective())
		{
			LevelVC->SetViewLocationForOrbiting(CameraLocation);
			LevelVC->SetViewLocation(CameraLocation);
			LevelVC->SetViewRotation(CameraRotation);

			break;
		}
	}
}

UWorld* UUnrealEditorSubsystem::GetEditorWorld()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return nullptr;
	}

	return InternalUnrealEditorSubsystemLibrary::GetEditorWorld();
}

UWorld* UUnrealEditorSubsystem::GetGameWorld()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	return InternalUnrealEditorSubsystemLibrary::GetGameWorld();
}