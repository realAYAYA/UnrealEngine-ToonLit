// Copyright Epic Games, Inc. All Rights Reserved.


#include "TabSpawner.h"
#include "Framework/Docking/TabManager.h"
UTabSpawner::UTabSpawner()
{
	Name="Tab Spawner";
	Tooltip="Allow you to spawn any tab by name";
	Category="Utility";
}

void UTabSpawner::Execute()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TabName);
}
