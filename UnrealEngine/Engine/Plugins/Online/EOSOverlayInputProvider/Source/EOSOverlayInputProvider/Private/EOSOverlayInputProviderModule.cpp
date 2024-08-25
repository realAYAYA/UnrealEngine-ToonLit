// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSOverlayInputProviderModule.h"

#include "CoreGlobals.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#include "EOSOverlayInputProviderPreProcessor.h"

IMPLEMENT_MODULE(FEOSOverlayInputProviderModule, EOSOverlayInputProvider);

void FEOSOverlayInputProviderModule::StartupTicker()
{
	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FEOSOverlayInputProviderModule::Tick), 0);
	}
}

void FEOSOverlayInputProviderModule::ShutdownTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

bool FEOSOverlayInputProviderModule::Tick(float DeltaTime)
{
	if (IsRenderReady())
	{
		ShutdownTicker();
	}

	return true;
}

bool FEOSOverlayInputProviderModule::IsRenderReady()
{
	if (bRenderReady)
	{
		return true;
	}

	// The FApp check ensures that we don't register the Input Processor in the wrong places, like the UE Editor Project list window
	if (!IsRunningCommandlet() && FApp::HasProjectName())
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		if (!FSlateApplication::Get().GetRenderer())
		{
			return false;
		}

		InputPreprocessor = MakeShared<FEOSOverlayInputProviderPreProcessor>();

		// Store a pointer to the processor in your module
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor, 0);
	}

	bRenderReady = true;

	return true;
}

void FEOSOverlayInputProviderModule::StartupModule()
{
	StartupTicker();
}

void FEOSOverlayInputProviderModule::ShutdownModule()
{
	if (!IsRunningCommandlet())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputPreprocessor);
		}
	}

	InputPreprocessor.Reset();

	ShutdownTicker();
}