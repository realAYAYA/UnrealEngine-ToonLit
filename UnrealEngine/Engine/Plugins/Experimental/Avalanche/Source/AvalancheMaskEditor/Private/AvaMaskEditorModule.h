// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#include "Framework/Commands/UICommandList.h"
#include "Input/Reply.h"
#include "Templates/SharedPointerFwd.h"

class SWidget;

class FAvalancheMaskEditorModule
	: public IModuleInterface
{
public:	
    // ~Begin IModuleInterface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    // ~End IModuleInterface

    TSharedPtr<FUICommandList> GetCommandList() const;
	
private:
	void RegisterMenus();	
	void ToggleEditorMode();

	TSharedRef<SWidget> GetStatusBarWidgetMenuContent();
	FReply OnToggleMaskModeClicked();

private:
	TSharedPtr<FUICommandList> CommandList;
};
