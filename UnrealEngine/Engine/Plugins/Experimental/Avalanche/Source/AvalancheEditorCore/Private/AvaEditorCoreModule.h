// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaEditorCoreModule.h"

class FAvaEditorCoreModule : public IAvaEditorCoreModule
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	//~ Begin IAvaEditorCoreModule
	virtual FOnExtendEditorToolbar& GetOnExtendEditorToolbar();
	//~ End IAvaEditorCoreModule

private:
	FOnExtendEditorToolbar OnExtendEditorToolbar;
};
