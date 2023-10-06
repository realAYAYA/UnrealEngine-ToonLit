// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTraceUtilities, Log, All)

class FEditorTraceUtilitiesModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
	static const FString& GetTraceUtilitiesIni() { return EditorTraceUtilitiesIni; }

private:
    FDelegateHandle RegisterStartupCallbackHandle;
	static FString EditorTraceUtilitiesIni;
};
