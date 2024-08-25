// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChooserTraceModule.h"
#include "Modules/ModuleInterface.h"
#include "RewindDebuggerChooser.h"

namespace UE::ChooserEditor
{

class FModule : public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	FRewindDebuggerChooser RewindDebuggerChooser;
	FChooserTraceModule ChooserTraceModule;
};

}