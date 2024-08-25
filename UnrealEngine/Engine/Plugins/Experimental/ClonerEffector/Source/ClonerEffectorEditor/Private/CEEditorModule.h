// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FCEEditorModule : public IModuleInterface
{
public:
	static inline constexpr TCHAR PropertyEditorName[] = TEXT("PropertyEditor");

	//~ Begin IModuleInterface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
