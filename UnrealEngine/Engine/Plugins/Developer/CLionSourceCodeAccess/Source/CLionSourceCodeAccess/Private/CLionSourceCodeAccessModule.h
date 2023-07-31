// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "CLionSourceCodeAccessor.h"

class FCLionSourceCodeAccessModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	virtual bool SupportsDynamicReloading() override;

	FCLionSourceCodeAccessor& GetAccessor();
private:

	FCLionSourceCodeAccessor CLionSourceCodeAccessor;

};