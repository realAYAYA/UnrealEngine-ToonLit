// Copyright (C) 2019 GameSeed - All Rights Reserved

#include "ZRedisClientPrivate.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogZRedis);

#define LOCTEXT_NAMESPACE "FZRedisClientModule"

class FZRedisClientModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
	}
	
	virtual void ShutdownModule() override
	{
	}
};

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FZRedisClientModule, ZRedisClient)
