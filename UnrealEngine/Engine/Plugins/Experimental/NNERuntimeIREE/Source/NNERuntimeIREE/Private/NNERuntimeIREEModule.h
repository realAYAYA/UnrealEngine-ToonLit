// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#ifdef WITH_NNE_RUNTIME_IREE
#include "NNERuntimeIREE.h"
#include "UObject/WeakObjectPtrTemplates.h"
#endif // WITH_NNE_RUNTIME_IREE

class FNNERuntimeIREEModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#ifdef WITH_NNE_RUNTIME_IREE
private:
	TWeakObjectPtr<UNNERuntimeIREECpu> NNERuntimeIREECpu;
	TWeakObjectPtr<UNNERuntimeIREECuda> NNERuntimeIREECuda;
	TWeakObjectPtr<UNNERuntimeIREEVulkan> NNERuntimeIREEVulkan;
	TWeakObjectPtr<UNNERuntimeIREERdg> NNERuntimeIREERdg;
#endif // WITH_NNE_RUNTIME_IREE
};
