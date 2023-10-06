// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangePipelinesModule.h"

#include "CoreMinimal.h"
#include "InterchangePipelineLog.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogInterchangePipeline);

class FInterchangePipelinesModule : public IInterchangePipelinesModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FInterchangePipelinesModule, InterchangePipelines)
