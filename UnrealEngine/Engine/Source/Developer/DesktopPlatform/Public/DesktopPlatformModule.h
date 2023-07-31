// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDesktopPlatform.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IDesktopPlatform;

class FDesktopPlatformModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	static IDesktopPlatform* Get()
	{
		FDesktopPlatformModule& DesktopPlatformModule = FModuleManager::Get().LoadModuleChecked<FDesktopPlatformModule>("DesktopPlatform");
		return DesktopPlatformModule.GetSingleton();
	}

private:
	virtual IDesktopPlatform* GetSingleton() const { return DesktopPlatform; }

	IDesktopPlatform* DesktopPlatform;
};
