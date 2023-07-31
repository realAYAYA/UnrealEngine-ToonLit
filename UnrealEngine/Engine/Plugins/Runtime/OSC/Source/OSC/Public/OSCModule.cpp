// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "OSCLog.h"

DEFINE_LOG_CATEGORY(LogOSC);


class FOSCModule : public IModuleInterface
{
public:
	virtual void StartupModule()
	{
		if (!FModuleManager::Get().LoadModule(TEXT("Networking")))
		{
			UE_LOG(LogOSC, Error, TEXT("Required module 'Networking' failed to load. OSC service disabled."));
		}
	}
};

IMPLEMENT_MODULE(FOSCModule, OSC)
