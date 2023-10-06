// Copyright Epic Games, Inc. All Rights Reserved.


#include "ChangeViewMode.h"

#include "LevelEditorViewport.h"

void UChangeViewMode::Execute()
{
	const TArray<FLevelEditorViewportClient*>& VCs=GEditor->GetLevelViewportClients();
	for (FLevelEditorViewportClient* VC:VCs)
	{
		VC->SetViewMode(ViewMode);
	}
		
	
	return Super::Execute();
}

UChangeViewMode::UChangeViewMode()
{
	Name="Change Viewport mode";
	Category="Viewport";
	Tooltip="Change the main viewport mode ( lit, unlit ...)";
	
}
