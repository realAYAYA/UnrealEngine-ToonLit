// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

UNDOHISTORYEDITOR_API extern const FName UndoHistoryTabName;

class UNDOHISTORYEDITOR_API IUndoHistoryEditorModule : public IModuleInterface
{
public:

	static IUndoHistoryEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IUndoHistoryEditorModule>("UndoHistoryEditor");
	}

	virtual void ExecuteOpenUndoHistory() = 0;
};
