// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UNNERuntimeORTDmlImpl;

class FNNERuntimeORTModule : public IModuleInterface
{

public:
	TWeakObjectPtr<UNNERuntimeORTDmlImpl> NNERuntimeORTDml{ nullptr };

	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void* OrtLibHandle{};
};