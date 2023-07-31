// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPixWinPlugin.h"

class FAutoConsoleCommand;
namespace Impl { class FPixDummyInputDevice; }
namespace Impl { class FPixGraphicsAnalysisInterface; }

/** PIX capture plugin implementation. */
class FPixWinPluginModule : public IPixWinPlugin
{
	friend Impl::FPixDummyInputDevice;

public:
	// Begin IRenderCaptureProvider interface.
	virtual void CaptureFrame(FViewport* InViewport, uint32 InFlags, FString const& InDestFileName) override;
	virtual void BeginCapture(FRHICommandListImmediate* InRHICommandList, uint32 InFlags, FString const& InDestFileName) override;
	virtual void EndCapture(FRHICommandListImmediate* InRHICommandList) override;
	// End IRenderCaptureProvider interface.

protected:
	// Begin IModuleInterface interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface interface.

	// Begin IInputDeviceModule interface.
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	// End IInputDeviceModule interface.

private:
	/** Tick to handle full frame capture events. */
	void Tick(float DeltaTime);

	/** Helper function for CVar command binding. */
	void CaptureFrame() { return CaptureFrame(nullptr, 0, FString()); }

	Impl::FPixGraphicsAnalysisInterface* PixGraphicsAnalysisInterface = nullptr;
	FAutoConsoleCommand* ConsoleCommandCaptureFrame = nullptr;

	bool bBeginCaptureNextTick = false;
	bool bEndCaptureNextTick = false;
};
