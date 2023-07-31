// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "DataInterfaceParam.h"

namespace UE::DataInterface
{

class FModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			FParamType::FRegistrar::RegisterDeferredTypes();
		});
	}
};

}

IMPLEMENT_MODULE(UE::DataInterface::FModule, DataInterface)