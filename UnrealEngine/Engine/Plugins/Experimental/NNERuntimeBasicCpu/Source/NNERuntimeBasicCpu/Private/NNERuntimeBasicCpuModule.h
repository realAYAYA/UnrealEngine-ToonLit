// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UNNERuntimeBasicCpuImpl;

class FNNERuntimeBasicCpuModule : public IModuleInterface
{
public:
	TWeakObjectPtr<UNNERuntimeBasicCpuImpl> NNERuntimeBasicCpu{ nullptr };

	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};