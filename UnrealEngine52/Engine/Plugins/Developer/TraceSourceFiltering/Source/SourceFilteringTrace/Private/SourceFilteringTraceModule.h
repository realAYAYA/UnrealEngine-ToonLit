// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"


class FSourceFilteringTraceModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
protected:
	void TraceFilterClasses();
#if WITH_EDITOR	
	void HandleNewFilterBlueprintCreated(class UBlueprint* InBlueprint);
#endif // WITH_EDITOR
};
