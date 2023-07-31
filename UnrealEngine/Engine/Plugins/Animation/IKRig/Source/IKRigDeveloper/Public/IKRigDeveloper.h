// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IKRIGDEVELOPER_API FIKRigDeveloperModule : public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;
};