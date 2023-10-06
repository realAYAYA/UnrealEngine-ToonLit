// Copyright Epic Games, Inc. All Rights Reserved.

#include "PointCloud.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogPointCloud);

class FPointCloudModule :
	public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FPointCloudModule, PointCloud);

void FPointCloudModule::StartupModule()
{
}

void FPointCloudModule::ShutdownModule()
{
}
