// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Net/Core/Misc/NetCoreLog.h"

#include "HAL/IConsoleManager.h"
#include "Net/Serialization/FastArraySerializer.h"

TAutoConsoleVariable<int32> CVarNetEnableDetailedScopeCounters(TEXT("net.EnableDetailedScopeCounters"), 1, TEXT("Enables detailed networking scope cycle counters. There are often lots of these which can negatively impact performance."));

DEFINE_LOG_CATEGORY(LogNetFastTArray);

DEFINE_LOG_CATEGORY(LogNetCore);


class FNetCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};


IMPLEMENT_MODULE(FNetCoreModule, NetCore);
