// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SubclassOf.h"


class UOptimusComputeDataInterface;


class IOptimusCoreModule : public IModuleInterface
{
public:
	/** Register a data interface after the OptimusCore module has loaded */
	template<typename T>
	bool RegisterDataInterfaceClass()
	{
		return RegisterDataInterfaceClass(T::StaticClass());
	}
	
protected:
	virtual bool RegisterDataInterfaceClass(TSubclassOf<UOptimusComputeDataInterface> InDataInterfaceClass) = 0;
};
