// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServices.h"

#include "Online/OnlineServicesRegistry.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

namespace UE::Online {

int32 GetBuildUniqueId()
{
	static bool bStaticCheck = false;
	static int32 BuildId = 0;
	static bool bUseBuildIdOverride = false;
	static int32 BuildIdOverride = 0;

	// add a cvar so it can be modified at runtime
	static FAutoConsoleVariableRef CVarBuildIdOverride(
		TEXT("buildidoverride"), BuildId,
		TEXT("Sets build id used for matchmaking "),
		ECVF_Default);

	if (!bStaticCheck)
	{
		bStaticCheck = true;
		if (FParse::Value(FCommandLine::Get(), TEXT("BuildIdOverride="), BuildIdOverride) && BuildIdOverride != 0)
		{
			bUseBuildIdOverride = true;
		}
		else
		{
			if (!GConfig->GetBool(TEXT("OnlineServices"), TEXT("bUseBuildIdOverride"), bUseBuildIdOverride, GEngineIni))
			{
				UE_LOG(LogTemp, Warning, TEXT("Missing bUseBuildIdOverride= in [OnlineServices] of DefaultEngine.ini"));
			}

			if (!GConfig->GetInt(TEXT("OnlineServices"), TEXT("BuildIdOverride"), BuildIdOverride, GEngineIni))
			{
				UE_LOG(LogTemp, Warning, TEXT("Missing BuildIdOverride= in [OnlineServices] of DefaultEngine.ini"));
			}
		}

		if (bUseBuildIdOverride == false)
		{
			// Removed old hashing code to use something more predictable and easier to override for when
			// it's necessary to force compatibility with an older build
			BuildId = FNetworkVersion::GetNetworkCompatibleChangelist();
		}
		else
		{
			BuildId = BuildIdOverride;
		}
	}

	return BuildId;
}

bool IsLoaded(EOnlineServices OnlineServices, FName InstanceName)
{
	return FOnlineServicesRegistry::Get().IsLoaded(OnlineServices, InstanceName);
}

TSharedPtr<IOnlineServices> GetServices(EOnlineServices OnlineServices, FName InstanceName)
{
	return FOnlineServicesRegistry::Get().GetNamedServicesInstance(OnlineServices, InstanceName);
}

void DestroyService(EOnlineServices OnlineServices, FName InstanceName)
{
	FOnlineServicesRegistry::Get().DestroyNamedServicesInstance(OnlineServices, InstanceName);
}

void DestroyAllNamedServices(EOnlineServices OnlineServices)
{
	FOnlineServicesRegistry::Get().DestroyAllNamedServicesInstances(OnlineServices);
}

/* UE::Online */ }
