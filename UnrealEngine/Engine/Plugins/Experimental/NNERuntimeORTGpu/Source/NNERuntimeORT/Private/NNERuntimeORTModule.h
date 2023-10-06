// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UNNERuntimeORTGpuImpl;

class FNNERuntimeORTModule : public IModuleInterface
{

public:
	TWeakObjectPtr<UNNERuntimeORTGpuImpl> NNERuntimeORTDml{ nullptr };
	TWeakObjectPtr<UNNERuntimeORTGpuImpl> NNERuntimeORTCuda{ nullptr };

	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TArray<void*> DllHandles;
};