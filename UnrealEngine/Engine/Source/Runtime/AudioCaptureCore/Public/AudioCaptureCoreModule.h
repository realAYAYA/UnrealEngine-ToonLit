// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


/* Public dependencies
*****************************************************************************/

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FAudioCaptureCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
