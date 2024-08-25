// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "IGeForceNOWWrapperModule.h"
#include "GeForceNOWWrapperPrivate.h"

#if NV_GEFORCENOW
#include "GeForceNOWWrapper.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY(LogGeForceNow);

class FGeForceNOWWrapperModule : public IGeForceNOWWrapperModule
{
public:

	FGeForceNOWWrapperModule(){}

	virtual void StartupModule() override
	{
#if NV_GEFORCENOW
		const GfnRuntimeError GfnResult = GeForceNOWWrapper::Initialize();
		const bool bGfnRuntimeSDKInitialized = GfnResult == gfnSuccess || GfnResult == gfnInitSuccessClientOnly;
		if (bGfnRuntimeSDKInitialized)
		{
			UE_LOG(LogGeForceNow, Log, TEXT("GeForceNow SDK initialized: %d"), (int32)GfnResult);
			const bool bIsRunningInTheCloud = GeForceNOWWrapper::Get().IsRunningInCloud();
			UE_LOG(LogGeForceNow, Log, TEXT("GeForceNow IsRunningInCloud %d"), bIsRunningInTheCloud ? 1 : 0);
			if (bIsRunningInTheCloud)
			{
				if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.WarnOfBadDrivers")))
				{
					CVar->Set(TEXT("0"));
				}
				if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.D3D12.DXR.MinimumWindowsBuildVersion")))
				{
					CVar->Set(TEXT("0"));
				}
				if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.IgnorePerformanceModeCheck")))
				{
					CVar->Set(true);
				}
			}
		}
		else
		{
			UE_LOG(LogGeForceNow, Log, TEXT("GeForceNow SDK initialization failed: %d"), (int32)GfnResult);
		}
#endif // NV_GEFORCENOW
	}

	virtual void ShutdownModule() override
	{
#if NV_GEFORCENOW
		GeForceNOWWrapper::Shutdown();
#endif // NV_GEFORCENOW
	}
};

IMPLEMENT_MODULE(FGeForceNOWWrapperModule, GeForceNOWWrapper);
