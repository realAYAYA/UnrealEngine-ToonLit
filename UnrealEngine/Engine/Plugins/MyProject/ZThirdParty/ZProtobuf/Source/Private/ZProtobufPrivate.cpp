// Copyright (C) 2019 GameSeed - All Rights Reserved

#include "ZProtobufPrivate.h"
#include "Modules/ModuleManager.h"


#define LOCTEXT_NAMESPACE "FZProtobufModule"

class FZProtobufModule : public IModuleInterface
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
	
IMPLEMENT_MODULE(FZProtobufModule, ZProtobuf)
