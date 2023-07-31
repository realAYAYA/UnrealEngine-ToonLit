// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/OnlineEngineInterface.h"
#include "UObject/Package.h"
#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OnlineEngineInterface)

UOnlineEngineInterface* UOnlineEngineInterface::Singleton = nullptr;

UOnlineEngineInterface::UOnlineEngineInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UOnlineEngineInterface* UOnlineEngineInterface::Get()
{
	if (!Singleton)
	{
		bool bUseOnlineServicesV2 = false;
		GConfig->GetBool(TEXT("/Script/Engine.OnlineEngineInterface"), TEXT("bUseOnlineServicesV2"), bUseOnlineServicesV2, GEngineIni);
		// Proper interface class hard coded here to emphasize the fact that this is not expected to change much, any need to do so should go through the OGS team first
		UClass* OnlineEngineInterfaceClass = nullptr;
		if (bUseOnlineServicesV2)
		{
			OnlineEngineInterfaceClass = StaticLoadClass(UOnlineEngineInterface::StaticClass(), NULL, TEXT("/Script/OnlineSubsystemUtils.OnlineServicesEngineInterfaceImpl"), NULL, LOAD_Quiet, NULL);
		}
		else
		{
			OnlineEngineInterfaceClass = StaticLoadClass(UOnlineEngineInterface::StaticClass(), NULL, TEXT("/Script/OnlineSubsystemUtils.OnlineEngineInterfaceImpl"), NULL, LOAD_Quiet, NULL);
		}
		if (!OnlineEngineInterfaceClass)
		{
			// Default to the no op class if necessary
			OnlineEngineInterfaceClass = UOnlineEngineInterface::StaticClass();
		}

		Singleton = NewObject<UOnlineEngineInterface>(GetTransientPackage(), OnlineEngineInterfaceClass);
		Singleton->AddToRoot();
	}

	return Singleton;
}

