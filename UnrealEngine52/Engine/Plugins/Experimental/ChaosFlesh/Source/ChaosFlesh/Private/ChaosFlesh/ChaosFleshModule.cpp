// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosFlesh/ChaosFleshModule.h"
#include "Modules/ModuleManager.h"

//////////////////////////////////////////////////////////////////////////
// FChaosFleshModule

class FChaosFleshModule : public IChaosFleshModuleInterface
{
  public:
    virtual void StartupModule() override
    {
        check(GConfig);
    }

    virtual void ShutdownModule() override
    {
    }

};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FChaosFleshModule, ChaosFlesh);
