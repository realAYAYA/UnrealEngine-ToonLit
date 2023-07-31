// Copyright Epic Games, Inc. All Rights Reserved.
#include "TestCommon/CoreUtilities.h"
#include "TestCommon/CoreUObjectUtilities.h"

#include "LowLevelTestModule.h"
#include "Modules/ModuleManager.h"

class FGlobalLLTModule : public ILowLevelTestsModule
{
public:
	virtual void GlobalSetup() override;
	virtual void GlobalTeardown() override;
};

IMPLEMENT_MODULE(FGlobalLLTModule, GlobalLowLevelTests);

void FGlobalLLTModule::GlobalSetup()
{
	InitTaskGraph();
	InitThreadPool(true);
#if WITH_COREUOBJECT
	InitCoreUObject();
#endif
}

void FGlobalLLTModule::GlobalTeardown()
{
#if WITH_COREUOBJECT
	CleanupCoreUObject();
#endif
	CleanupThreadPool();
	CleanupTaskGraph();
}
