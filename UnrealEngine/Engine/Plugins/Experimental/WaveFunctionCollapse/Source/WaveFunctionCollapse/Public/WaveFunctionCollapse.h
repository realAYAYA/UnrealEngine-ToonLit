// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FMenuBuilder;
class FReply;

class FWaveFunctionCollapseModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void AddMenuEntry(FMenuBuilder& MenuBuilder);
	void WaveFunctionCollapseUI();

	TSharedPtr<class FUICommandList> PluginCommands;
};