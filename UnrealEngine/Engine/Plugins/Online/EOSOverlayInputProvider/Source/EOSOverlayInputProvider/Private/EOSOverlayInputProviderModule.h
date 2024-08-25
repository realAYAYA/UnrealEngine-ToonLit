// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "EOSOverlayInputProviderPreProcessor.h"

class FEOSOverlayInputProviderModule : public IModuleInterface
{
public:
	FEOSOverlayInputProviderModule() = default;
	~FEOSOverlayInputProviderModule() = default;

private:
	void StartupTicker();
	void ShutdownTicker();
	bool Tick(float);

	// ~Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// ~End IModuleInterface

	bool IsRenderReady();

private:
	FTSTicker::FDelegateHandle TickerHandle;

	bool bRenderReady = false;

	TSharedPtr<FEOSOverlayInputProviderPreProcessor> InputPreprocessor;
};