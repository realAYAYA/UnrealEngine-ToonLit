// Copyright Epic Games, Inc. All Rights Reserved.

#include "Niagara/NiagaraSimCachingPlugin.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(INiagaraSimCachingPlugin, NiagaraSimCaching)

DEFINE_LOG_CATEGORY(LogNiagaraSimCaching)

void INiagaraSimCachingPlugin::StartupModule()
{
}

void INiagaraSimCachingPlugin::ShutdownModule() 
{
}
