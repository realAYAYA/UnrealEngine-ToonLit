// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UNNERuntimeRDGDmlImpl;
class UNNERuntimeRDGHlslImpl;

class FNNERuntimeRDGModule : public IModuleInterface
{

public:
	TWeakObjectPtr<UNNERuntimeRDGHlslImpl> NNERuntimeRDGHlsl{ nullptr };
	TWeakObjectPtr<UNNERuntimeRDGDmlImpl> NNERuntimeRDGDml{ nullptr };

	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};