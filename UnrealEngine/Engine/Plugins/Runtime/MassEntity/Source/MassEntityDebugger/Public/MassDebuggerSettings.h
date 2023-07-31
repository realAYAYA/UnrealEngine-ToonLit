// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassSettings.h"
#include "MassDebuggerSettings.generated.h"


#define GET_MASSDEBUGGER_CONFIG_VALUE(a) (GetMutableDefault<UMassDebuggerSettings>()->a)

UCLASS(config = EditorPerProjectUserSettings, DisplayName = "Mass Debugger")
class MASSENTITYDEBUGGER_API UMassDebuggerSettings : public UMassModuleSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category=Debugger)
	bool bStripMassPrefix = true;
};
