// Copyright (C) 2019 GameSeed - All Rights Reserved

#include "Modules/ModuleManager.h"


#define LOCTEXT_NAMESPACE "FZRpcModule"

class FZRpcModule : public IModuleInterface
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
	
IMPLEMENT_MODULE(FZRpcModule, ZRpc)
