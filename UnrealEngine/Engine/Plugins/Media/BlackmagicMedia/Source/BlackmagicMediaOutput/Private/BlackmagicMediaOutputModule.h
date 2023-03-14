// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlackmagicMediaOutput, Log, All);


class FBlackmagicMediaOutputModule : public IModuleInterface
{
public:
	static FBlackmagicMediaOutputModule& Get();
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	bool IsGPUTextureTransferAvailable() const;

private:
	bool bIsGPUTextureTransferAvailable = false;
};