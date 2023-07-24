// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Docking/TabManager.h"
#include "RewindDebuggerCamera.h"

class SRewindDebugger;
class SRewindDebuggerDetails;

class FRewindDebuggerModule : public IModuleInterface
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedRef<SDockTab> SpawnRewindDebuggerTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<SDockTab> SpawnRewindDebuggerDetailsTab(const FSpawnTabArgs& SpawnTabArgs);

	static const FName MainTabName;
	static const FName DetailsTabName;

private:
	TSharedPtr<SRewindDebugger> RewindDebuggerWidget;
	TSharedPtr<SRewindDebuggerDetails> RewindDebuggerDetailsWidget;

	FRewindDebuggerCamera RewindDebuggerCameraExtension;
};
