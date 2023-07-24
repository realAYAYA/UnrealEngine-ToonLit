// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FTraceAnalysisModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	static FName GetMessageLogName();
};