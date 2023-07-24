// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


class DMXCONTROLCONSOLEEDITOR_API IDMXControlConsoleEditorModule
	: public IModuleInterface
{
public:
	static IDMXControlConsoleEditorModule& Get()
	{
		return FModuleManager::Get().GetModuleChecked<IDMXControlConsoleEditorModule>("DMXControlConsoleEditor");
	}
};
