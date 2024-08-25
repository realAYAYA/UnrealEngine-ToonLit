// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SubclassOf.h"
#include "OptimusFunctionNodeGraph.h"
#include "OptimusNode.h"


class UOptimusComputeDataInterface;


class IOptimusCoreModule : public IModuleInterface
{
public:
	static IOptimusCoreModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IOptimusCoreModule >(TEXT("OptimusCore"));
	}
	
	/** Register a data interface after the OptimusCore module has loaded */
	template<typename T>
	bool RegisterDataInterfaceClass()
	{
		return RegisterDataInterfaceClass(T::StaticClass());
	}
	
	virtual void UpdateFunctionReferences(const FSoftObjectPath& InOldGraphPath, const FSoftObjectPath& InNewGraphPath) = 0;
	
protected:
	virtual bool RegisterDataInterfaceClass(TSubclassOf<UOptimusComputeDataInterface> InDataInterfaceClass) = 0;
};
