// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixWinPluginModule.h"

#include "CoreMinimal.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"

#if !defined(WITH_PIX_EVENT_RUNTIME)
#define WITH_PIX_EVENT_RUNTIME 0
#endif

#define PIX_PLUGIN_ENABLED (WITH_PIX_EVENT_RUNTIME && !UE_BUILD_SHIPPING)

#if PIX_PLUGIN_ENABLED
#define USE_PIX 1
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include <pix3.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(PixWinPlugin, Log, All);

#define LOCTEXT_NAMESPACE "PixWinPlugin"

namespace Impl
{
	/** Container for graphics analysis com interface. */
	class FPixGraphicsAnalysisInterface
	{
	public:
		FPixGraphicsAnalysisInterface()
		{
#if PIX_PLUGIN_ENABLED
			WinPixGpuCapturerHandle = FPlatformProcess::GetDllHandle(L"WinPixGpuCapturer.dll");

			if (!WinPixGpuCapturerHandle)
			{
				if (FParse::Param(FCommandLine::Get(), TEXT("attachPIX")))
				{
					WinPixGpuCapturerHandle = PIXLoadLatestWinPixGpuCapturerLibrary();
				}
			}

			if (WinPixGpuCapturerHandle)
			{
				PIXSetHUDOptions(PIX_HUD_SHOW_ON_NO_WINDOWS);
			}
#endif // PIX_PLUGIN_ENABLED
		}

		bool IsValid()
		{
			return WinPixGpuCapturerHandle != nullptr;
		}

		void BeginCapture(void* WindowHandle, const FString& DestFileName)
		{
#if PIX_PLUGIN_ENABLED
			if (WinPixGpuCapturerHandle)
			{
				PIXSetTargetWindow((HWND)WindowHandle);

				PIXCaptureParameters Parameters{};
				// UETODO: christopher.waters - implement capturing to a file.
				//Parameters.GpuCaptureParameters.FileName = *DestFileName;
				Parameters.GpuCaptureParameters.FileName = TEXT("");

				PIXBeginCapture2(PIX_CAPTURE_GPU, &Parameters);
			}
#endif
		}

		void EndCapture()
		{
#if PIX_PLUGIN_ENABLED
			if (WinPixGpuCapturerHandle)
			{
				PIXEndCapture(0);
			}
#endif
		}

	private:
		void* WinPixGpuCapturerHandle{};
	};


	/** Dummy input device that is used only to generate a Tick. */
	class FPixDummyInputDevice : public IInputDevice
	{
	public:
		FPixDummyInputDevice(FPixWinPluginModule* InModule)
			: Module(InModule)
		{ }

		virtual void Tick(float DeltaTime) override
		{
			if (ensure(Module != nullptr))
			{
				Module->Tick(DeltaTime);
			}
		}

		virtual void SendControllerEvents() override { }
		virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override { }
		virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return(false); }
		virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override { }
		virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& values) override { }

	private:
		FPixWinPluginModule* Module;
	};
}

void FPixWinPluginModule::StartupModule()
{
	PixGraphicsAnalysisInterface = new Impl::FPixGraphicsAnalysisInterface();
	if (PixGraphicsAnalysisInterface->IsValid())
	{
		// Register modular features.
		IModularFeatures::Get().RegisterModularFeature(IRenderCaptureProvider::GetModularFeatureName(), (IRenderCaptureProvider*)this);
		IModularFeatures::Get().RegisterModularFeature(IInputDeviceModule::GetModularFeatureName(), (IInputDeviceModule*)this);

		// Register console command.
		ConsoleCommandCaptureFrame = new FAutoConsoleCommand(
			TEXT("pix.GpuCaptureFrame"),
			TEXT("Captures the rendering commands of the next frame."),
			FConsoleCommandDelegate::CreateRaw(this, &FPixWinPluginModule::CaptureFrame));

		UE_LOG(PixWinPlugin, Log, TEXT("PIX capture plugin is ready!"));
	}
	else
	{
		UE_LOG(PixWinPlugin, Log, TEXT("PIX capture plugin failed to initialize! Check that the process is launched from PIX."));
	}
}

void FPixWinPluginModule::ShutdownModule()
{
	delete PixGraphicsAnalysisInterface;
	PixGraphicsAnalysisInterface = nullptr;
	delete ConsoleCommandCaptureFrame;
	ConsoleCommandCaptureFrame = nullptr;

	IModularFeatures::Get().UnregisterModularFeature(IRenderCaptureProvider::GetModularFeatureName(), (IRenderCaptureProvider*)this);
	IModularFeatures::Get().UnregisterModularFeature(IInputDeviceModule::GetModularFeatureName(), (IInputDeviceModule*)this);
}

TSharedPtr<class IInputDevice> FPixWinPluginModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	UE_LOG(PixWinPlugin, Log, TEXT("Creating dummy input device (for intercepting engine ticks)"));
	Impl::FPixDummyInputDevice* InputDev = new Impl::FPixDummyInputDevice(this);
	return MakeShareable((IInputDevice*)InputDev);
}

void FPixWinPluginModule::CaptureFrame(FViewport* InViewport, uint32 InFlags, FString const& InDestFileName)
{
	if (!bEndCaptureNextTick)
	{
		DoFrameCaptureCurrentViewport(InViewport, InFlags, InDestFileName);
	}
}

void FPixWinPluginModule::BeginCapture(FRHICommandListImmediate* InRHICommandList, uint32 InFlags, FString const& InDestFileName)
{
	InRHICommandList->SubmitCommandsAndFlushGPU();
	InRHICommandList->EnqueueLambda([Pix = PixGraphicsAnalysisInterface, InDestFileName](FRHICommandListImmediate& RHICommandList)
	{
		Pix->BeginCapture(nullptr, InDestFileName);
	});
}

void FPixWinPluginModule::EndCapture(FRHICommandListImmediate* InRHICommandList)
{
	InRHICommandList->SubmitCommandsAndFlushGPU();
	InRHICommandList->EnqueueLambda([Pix = PixGraphicsAnalysisInterface](FRHICommandListImmediate& RHICommandList)
	{
		Pix->EndCapture();
	});
}

void FPixWinPluginModule::Tick(float DeltaTime)
{
	if (bBeginCaptureNextTick)
	{
		// Start a capture.
		bBeginCaptureNextTick = false;
		bEndCaptureNextTick = true;

		ENQUEUE_RENDER_COMMAND(BeginCaptureCommand)([this](FRHICommandListImmediate& RHICommandList)
		{
			BeginCapture(&RHICommandList, 0, FString());
		});
	}
	else if (bEndCaptureNextTick)
	{
		// End a capture.
		bEndCaptureNextTick = false;

		ENQUEUE_RENDER_COMMAND(EndCaptureCommand)([this](FRHICommandListImmediate& RHICommandList)
		{
			EndCapture(&RHICommandList);
		});
	}
}

void FPixWinPluginModule::DoFrameCaptureCurrentViewport(FViewport* InViewport, uint32 InFlags, FString const& InDestFileName)
{
	// infer the intended viewport to intercept/capture:
	FViewport* Viewport = InViewport;

	check(GEngine);
	if (!Viewport && GEngine->GameViewport)
	{
		check(GEngine->GameViewport->Viewport);
		if (GEngine->GameViewport->Viewport->HasFocus())
		{
			Viewport = GEngine->GameViewport->Viewport;
		}
	}

#if WITH_EDITOR
	if (!Viewport && GEditor)
	{
		// WARNING: capturing from a "PIE-Eject" Editor viewport will not work as
		// expected; in such case, capture via the console command
		// (this has something to do with the 'active' editor viewport when the UI
		// button is clicked versus the one which the console is attached to)
		Viewport = GEditor->GetActiveViewport();
	}
#endif // WITH_EDITOR

	check(Viewport);
	BeginFrameCapture(Viewport->GetWindow(), InDestFileName);

	Viewport->Draw(true);

	EndFrameCapture();
}

void FPixWinPluginModule::BeginFrameCapture(void* HWnd, const FString& DestFileName)
{
	UE_LOG(PixWinPlugin, Log, TEXT("Capturing a frame in PIX"));

	ENQUEUE_RENDER_COMMAND(StartRenderDocCapture)(
		[this, HWnd, DestFileName](FRHICommandListImmediate& RHICmdList)
		{
			PixGraphicsAnalysisInterface->BeginCapture(HWnd, DestFileName);
		});
}

void FPixWinPluginModule::EndFrameCapture()
{
	ENQUEUE_RENDER_COMMAND(EndRenderDocCapture)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			PixGraphicsAnalysisInterface->EndCapture();
		});
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPixWinPluginModule, PixWinPlugin)
