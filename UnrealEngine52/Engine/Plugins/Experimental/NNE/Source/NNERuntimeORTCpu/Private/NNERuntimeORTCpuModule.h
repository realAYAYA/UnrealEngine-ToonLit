// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UNNERuntimeORTCpuImpl;

class FNNERuntimeORTCpuModule : public IModuleInterface
{

public:
	TWeakObjectPtr<UNNERuntimeORTCpuImpl> NNERuntimeORTCpu{ nullptr };

	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};