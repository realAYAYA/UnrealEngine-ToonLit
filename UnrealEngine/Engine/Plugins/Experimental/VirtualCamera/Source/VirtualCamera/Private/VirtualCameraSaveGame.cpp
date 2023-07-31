// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraSaveGame.h"

int32 FVirtualCameraSettingsPreset::NextIndex = 1;
int32 FVirtualCameraWaypoint::NextIndex = 1;
int32 FVirtualCameraScreenshot::NextIndex = 1;

UVirtualCameraSaveGame::UVirtualCameraSaveGame(const FObjectInitializer& ObjectInitializer)
{
	LLM_SCOPE_BYNAME("VirtualCamera/VirtualCameraSaveGame");
	SaveSlotName = "SavedVirtualCameraSettings";
	UserIndex = 0;
}