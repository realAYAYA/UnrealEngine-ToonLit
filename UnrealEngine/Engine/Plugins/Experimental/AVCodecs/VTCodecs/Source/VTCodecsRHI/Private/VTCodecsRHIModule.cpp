// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "RHI.h"

#include "VT.h"
#include "AVUtility.h"


class FVTCodecRHI : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (FApp::CanEverRender())
        {
            FCoreDelegates::OnPostEngineInit.AddLambda([]()
                {
                    const_cast<FVT&>(FAPI::Get<FVT>()).bHasCompatibleGPU = IsRHIDeviceApple();
                });
        }
    }
};

IMPLEMENT_MODULE(FVTCodecRHI, VTCodecsRHI);
