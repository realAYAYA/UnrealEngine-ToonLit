// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemOculusModule.h"
#include "OnlineSubsystemOculusPrivate.h"

// FOnlineSubsystemOculusModule

IMPLEMENT_MODULE(FOnlineSubsystemOculusModule, OnlineSubsystemOculus);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryOculus : public IOnlineFactory
{
private:

	/** Singleton instance of the Oculus OSS */
	FOnlineSubsystemOculusPtr OnlineSub;

public:

	FOnlineFactoryOculus() {}	
	virtual ~FOnlineFactoryOculus() {}

	IOnlineSubsystemPtr CreateSubsystem(FName InstanceName) override
	{
		UE_LOG_ONLINE_ONCE(Warning, TEXT("OnlineSubsystemOculus has been deprecated"));

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (!OnlineSub.IsValid())
		{
			OnlineSub = MakeShared<FOnlineSubsystemOculus, ESPMode::ThreadSafe>(InstanceName);
		}
		if (OnlineSub->IsEnabled())
		{
			if (!OnlineSub->IsInitialized())
			{
				if (!OnlineSub->Init())
				{
					UE_LOG_ONLINE(Warning, TEXT("Oculus API failed to initialize!"));
					// Shutdown already called in Init() when this failed
					OnlineSub = nullptr;
				}
			}
			else
			{
				UE_LOG_ONLINE(Log, TEXT("Oculus API already initialized!"));
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Oculus API disabled!"));
			OnlineSub->Shutdown();
			OnlineSub = nullptr;
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return OnlineSub;
	}
};

void FOnlineSubsystemOculusModule::StartupModule()
{
	UE_LOG_ONLINE(Log, TEXT("Oculus Startup!"));

	OculusFactory = new FOnlineFactoryOculus();

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OSS.RegisterPlatformService(OCULUS_SUBSYSTEM, OculusFactory);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FOnlineSubsystemOculusModule::ShutdownModule()
{
	UE_LOG_ONLINE(Log, TEXT("Oculus Shutdown!"));

	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OSS.UnregisterPlatformService(OCULUS_SUBSYSTEM);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	delete OculusFactory;
	OculusFactory = nullptr;
}
