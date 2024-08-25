// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UNNERuntimeORTDml;
class UNNERuntimeORTCpu;

class FNNERuntimeORTModule : public IModuleInterface
{
private:
	TWeakObjectPtr<UNNERuntimeORTDml> NNERuntimeORTDml{ nullptr };
	TWeakObjectPtr<UNNERuntimeORTCpu> NNERuntimeORTCpu{ nullptr };

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TArray<void*> DllHandles;
};