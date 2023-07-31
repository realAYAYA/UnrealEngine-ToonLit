// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "RewindDebuggerVLog.h"
#include "VLogTraceModule.h"

class SRewindDebugger;

class FRewindDebuggerVLogModule : public IModuleInterface
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FRewindDebuggerVLog RewindDebuggerVLogExtension;
	FVLogTraceModule VLogTraceModule;
};
