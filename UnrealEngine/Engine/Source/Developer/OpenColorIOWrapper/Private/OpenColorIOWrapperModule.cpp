// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOWrapperModule.h"

#include "OpenColorIOWrapper.h"


DEFINE_LOG_CATEGORY(LogOpenColorIOWrapper);

#define OPENCOLORIOWRAPPER_MODULE_NAME "OpenColorIOWrapper"
#define LOCTEXT_NAMESPACE "OpenColorIOWrapperModule"

class FOpenColorIOWrapperModule : public IOpenColorIOWrapperModule
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
		EngineBuiltInConfig.Reset();
	}
	//~ End IModuleInterface interface

	//~ Begin IOpenColorIOWrapperModule interface
	virtual FOpenColorIOWrapperEngineConfig& GetEngineBuiltInConfig() override
	{
		FScopeLock Lock(&ConfigCriticalSection);

		if (!EngineBuiltInConfig)
		{
			EngineBuiltInConfig = MakeUnique<FOpenColorIOWrapperEngineConfig>();
		}

		return *EngineBuiltInConfig;
	}
	//~ End IOpenColorIOWrapperModule interface
private:

	// Critical section for the engine config getter.
	FCriticalSection ConfigCriticalSection;

	// Global engine config using the built-in studio config, lazily allocated.
	TUniquePtr<FOpenColorIOWrapperEngineConfig> EngineBuiltInConfig;
};

IMPLEMENT_MODULE(FOpenColorIOWrapperModule, OpenColorIOWrapper);

#undef LOCTEXT_NAMESPACE
