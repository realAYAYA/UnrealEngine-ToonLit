// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class EDITORPERFORMANCEMODULE_API IEditorPerformanceModule : public IModuleInterface
{
public:
	static FORCEINLINE IEditorPerformanceModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IEditorPerformanceModule>("EditorPerformanceModule");
	}
};