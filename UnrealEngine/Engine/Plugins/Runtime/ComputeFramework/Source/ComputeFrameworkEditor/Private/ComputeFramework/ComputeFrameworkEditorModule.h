// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/IComputeFrameworkEditorModule.h"
#include "ComputeFramework/ComputeFrameworkCompilationTick.h"

class FComputeFrameworkEditorModule : public IComputeFrameworkEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<class FComputeFrameworkCompilationTick> TickObject;
};
