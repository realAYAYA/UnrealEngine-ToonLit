// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLiveLinkVRPN, Log, All);

class FLiveLinkVRPNModule : public IModuleInterface
{
public:

	static FLiveLinkVRPNModule& Get()
	{
		return FModuleManager::Get().LoadModuleChecked<FLiveLinkVRPNModule>(TEXT("LiveLinkVRPN"));
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
