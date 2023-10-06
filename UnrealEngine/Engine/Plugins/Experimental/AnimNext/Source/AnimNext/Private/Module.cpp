// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "DataRegistry.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeTemplateRegistry.h"

namespace UE::AnimNext
{

class FModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FDataRegistry::Init();
		FDecoratorRegistry::Init();
		FNodeTemplateRegistry::Init();

		EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddLambda([]()
		{
			FDataRegistry::Destroy();
			FDecoratorRegistry::Destroy();
			FNodeTemplateRegistry::Destroy();
		});
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);
	}

	FDelegateHandle EnginePreExitHandle;
};

}

IMPLEMENT_MODULE(UE::AnimNext::FModule, AnimNext)
