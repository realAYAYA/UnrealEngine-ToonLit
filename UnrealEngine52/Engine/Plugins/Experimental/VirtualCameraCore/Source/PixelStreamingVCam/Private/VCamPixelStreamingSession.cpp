// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamPixelStreamingSession.h"

#include "Output/VCamOutputComposure.h"
#include "VCamComponent.h"
#include "VCamPixelStreamingSubsystem.h"
#include "VCamPixelStreamingLiveLink.h"

#include "Async/Async.h"
#include "Containers/UnrealString.h"
#include "Editor/EditorPerformanceSettings.h"
#include "IPixelStreamingModule.h"
#include "IPixelStreamingInputModule.h"
#include "Modules/ModuleManager.h"
#include "Math/Matrix.h"
#include "IPixelStreamingEditorModule.h"
#include "PixelStreamingVCamLog.h"
#include "PixelStreamingInputProtocol.h"
#include "PixelStreamingInputMessage.h"
#include "PixelStreamingServers.h"
#include "Serialization/MemoryReader.h"
#include "Slate/SceneViewport.h"
#include "VPFullScreenUserWidget.h"
#include "Widgets/SVirtualWindow.h"
#include "PixelStreamingInputEnums.h"

int UVCamPixelStreamingSession::NextDefaultStreamerId = 1;

namespace UE::VCamPixelStreamingSession::Private
{
	static const FSoftClassPath EmptyUMGSoftClassPath(TEXT("/VCamCore/Assets/VCam_EmptyVisibleUMG.VCam_EmptyVisibleUMG_C"));
} // namespace UE::VCamPixelStreamingSession::Private

UVCamPixelStreamingSession::UVCamPixelStreamingSession()
{
	DisplayType = EVPWidgetDisplayType::PostProcess;
	InitViewTargetPolicyInSubclass();
}

void UVCamPixelStreamingSession::Deinitialize()
{
	if (MediaOutput)
	{
		MediaOutput->ConditionalBeginDestroy();
	}
	MediaOutput = nullptr;
	Super::Deinitialize();
}

void UVCamPixelStreamingSession::Activate()
{
	if (!IsInitialized())
	{
		UE_LOG(LogPixelStreamingVCam, Warning, TEXT("Trying to start Pixel Streaming, but has not been initialized yet"));
		SetActive(false);
		return;
	}

	if (StreamerId.IsEmpty())
	{
		StreamerId = FString::Printf(TEXT("VCam%d"), NextDefaultStreamerId++);
	}

	// Rename the underlying session to the streamer name
	Rename(*StreamerId);

	// Setup livelink source
	UVCamPixelStreamingSubsystem::Get()->TryGetLiveLinkSource(this);

	if (UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get())
	{
		PixelStreamingSubsystem->RegisterActiveOutputProvider(this);
		UVCamComponent* VCamComponent = GetTypedOuter<UVCamComponent>();
		if (bAutoSetLiveLinkSubject && IsValid(VCamComponent))
		{
			VCamComponent->SetLiveLinkSubobject(GetFName());
		}
	}

	// If we don't have a UMG assigned, we still need to create an empty 'dummy' UMG in order to properly route the input back from the RemoteSession device
	if (!GetUMGClass())
	{
		bUsingDummyUMG = true;
		SetUMGClass(UE::VCamPixelStreamingSession::Private::EmptyUMGSoftClassPath.TryLoadClass<UUserWidget>());
	}

	// create a new media output if we dont already have one, or its not valid, or if the id has changed
	if (MediaOutput == nullptr || !MediaOutput->IsValid() || MediaOutput->GetStreamer()->GetId() != StreamerId)
	{
		MediaOutput = UPixelStreamingMediaOutput::Create(GetTransientPackage(), StreamerId);
		MediaOutput->OnRemoteResolutionChanged().AddUObject(this, &UVCamPixelStreamingSession::OnRemoteResolutionChanged);
	}

	UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	bOldThrottleCPUWhenNotForeground = Settings->bThrottleCPUWhenNotForeground;
	if (PreventEditorIdle)
	{
		Settings->bThrottleCPUWhenNotForeground = false;
		Settings->PostEditChange();
	}

	// This sets up media capture and streamer
	SetupCapture();

	// Super::Activate() creates our UMG which we need before setting up our custom input handling
	Super::Activate();

	// We setup custom handling of ARKit transforms coming from iOS devices here
	SetupCustomInputHandling();

	// We need signalling server to be up before we can start streaming
	SetupSignallingServer();

	if (MediaOutput->IsValid())
	{
		UE_LOG(LogPixelStreamingVCam, Log, TEXT("Activating PixelStreaming VCam Session. Endpoint: %s"), *MediaOutput->GetStreamer()->GetSignallingServerURL());
	}
}

void UVCamPixelStreamingSession::SetupCapture()
{
	UE_LOG(LogPixelStreamingVCam, Log, TEXT("Create new media capture for Pixel Streaming VCam."));

	if (MediaCapture)
	{
		MediaCapture->OnStateChangedNative.RemoveAll(this);
	}

	// Create a capturer that will capture frames from viewport and send them to streamer
	MediaCapture = Cast<UPixelStreamingMediaCapture>(MediaOutput->CreateMediaCapture());
	MediaCapture->OnStateChangedNative.AddUObject(this, &UVCamPixelStreamingSession::OnCaptureStateChanged);
	StartCapture();
}

void UVCamPixelStreamingSession::OnCaptureStateChanged()
{
	if (!MediaCapture || !MediaOutput || !MediaOutput->IsValid())
	{
		return;
	}

	switch (MediaCapture->GetState())
	{
		case EMediaCaptureState::Capturing:
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("Starting media capture and streaming for Pixel Streaming VCam."));
			MediaOutput->StartStreaming();
			break;
		case EMediaCaptureState::Stopped:
			if (MediaCapture->WasViewportResized())
			{
				UE_LOG(LogPixelStreamingVCam, Log, TEXT("Pixel Streaming VCam capture was stopped due to resize, going to restart capture."));
				// If it was stopped and viewport resized we assume resize caused the stop, so try a restart of capture here.
				SetupCapture();
			}
			else
			{
				UE_LOG(LogPixelStreamingVCam, Log, TEXT("Stopping media capture and streaming for Pixel Streaming VCam."));
				MediaOutput->StopStreaming();
			}
			break;
		case EMediaCaptureState::Error:
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("Pixel Streaming VCam capture hit an error, capturing will stop."));
			break;
		default:
			break;
	}
}

void UVCamPixelStreamingSession::OnRemoteResolutionChanged(const FIntPoint& RemoteResolution)
{
	// Early out if match remote resolution is not enabled.
	if (!bMatchRemoteResolution)
	{
		return;
	}

	// Ensure override resolution is being used
	if (!bUseOverrideResolution)
	{
		bUseOverrideResolution = true;
	}

	// Set the override resolution on the output provider base, this will trigger a resize
	OverrideResolution = RemoteResolution;
	ApplyOverrideResolutionForViewport(GetTargetViewport());
}

void UVCamPixelStreamingSession::SetupCustomInputHandling()
{
	if (GetUMGWidget())
	{
		TSharedPtr<SVirtualWindow> InputWindow;
		// If we are rendering from a ComposureOutputProvider, we need to get the InputWindow from that UMG, not the one in the PixelStreamingOutputProvider
		if (UVCamOutputComposure* ComposureProvider = Cast<UVCamOutputComposure>(GetOtherOutputProviderByIndex(FromComposureOutputProviderIndex)))
		{
			if (UVPFullScreenUserWidget* ComposureUMGWidget = ComposureProvider->GetUMGWidget())
			{
				InputWindow = ComposureUMGWidget->PostProcessDisplayType.GetSlateWindow();
				UE_LOG(LogPixelStreamingVCam, Log, TEXT("InputChannel callback - Routing input to active viewport with Composure UMG"));
			}
			else
			{
				UE_LOG(LogPixelStreamingVCam, Warning, TEXT("InputChannel callback - Composure usage was requested, but the specified ComposureOutputProvider has no UMG set"));
			}
		}
		else
		{
			InputWindow = GetUMGWidget()->PostProcessDisplayType.GetSlateWindow();
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("InputChannel callback - Routing input to active viewport with UMG"));
		}

		MediaOutput->GetStreamer()->SetTargetWindow(InputWindow);
		MediaOutput->GetStreamer()->SetInputHandlerType(EPixelStreamingInputType::RouteToWidget);
	}
	else
	{
		MediaOutput->GetStreamer()->SetTargetWindow(GetTargetInputWindow());
		MediaOutput->GetStreamer()->SetInputHandlerType(EPixelStreamingInputType::RouteToWidget);
		UE_LOG(LogPixelStreamingVCam, Log, TEXT("InputChannel callback - Routing input to active viewport"));
	}

	if (MediaOutput)
	{
		IPixelStreamingInputModule& PixelStreamingInputModule = IPixelStreamingInputModule::Get();
		typedef EPixelStreamingMessageTypes EType;
		/*
		 * ====================
		 * ARKit Transform
		 * ====================
		 */
		FPixelStreamingInputMessage ARKitMessage = FPixelStreamingInputMessage(100, { // 4x4 Transform
																						EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float, EType::Float,
																						// Timestamp
																						EType::Double });

		const TFunction<void(FMemoryReader)>& ARKitHandler = [this](FMemoryReader Ar) {
			if (!EnableARKitTracking)
			{
				return;
			}

			// The buffer contains the transform matrix stored as 16 floats
			FMatrix ARKitMatrix;
			for (int32 Row = 0; Row < 4; ++Row)
			{
				float Col0, Col1, Col2, Col3;
				Ar << Col0 << Col1 << Col2 << Col3;
				ARKitMatrix.M[Row][0] = Col0;
				ARKitMatrix.M[Row][1] = Col1;
				ARKitMatrix.M[Row][2] = Col2;
				ARKitMatrix.M[Row][3] = Col3;
			}
			ARKitMatrix.DiagnosticCheckNaN();

			// Extract timestamp
			double Timestamp;
			Ar << Timestamp;

			if (TSharedPtr<FPixelStreamingLiveLinkSource> LiveLinkSource = UVCamPixelStreamingSubsystem::Get()->TryGetLiveLinkSource(this))
			{
				LiveLinkSource->PushTransformForSubject(GetFName(), FTransform(ARKitMatrix), Timestamp);
			}
		};

		FPixelStreamingInputProtocol::ToStreamerProtocol.Add("ARKitTransform", ARKitMessage);
		if (TSharedPtr<IPixelStreamingInputHandler> InputHandler = MediaOutput->GetStreamer()->GetInputHandler().Pin())
		{
			InputHandler->RegisterMessageHandler("ARKitTransform", ARKitHandler);
		}
	}
	else
	{
		UE_LOG(LogPixelStreamingVCam, Error, TEXT("Failed to setup custom input handling."));
	}
}

void UVCamPixelStreamingSession::StartCapture()
{
	if (!MediaCapture)
	{
		return;
	}

	FMediaCaptureOptions Options;
	Options.bResizeSourceBuffer = true;
	Options.OverrunAction = EMediaCaptureOverrunAction::Skip;

	// If we are rendering from a ComposureOutputProvider, get the requested render target and use that instead of the viewport
	if (UVCamOutputComposure* ComposureProvider = Cast<UVCamOutputComposure>(GetOtherOutputProviderByIndex(FromComposureOutputProviderIndex)))
	{
		if (ComposureProvider->FinalOutputRenderTarget)
		{
			MediaCapture->CaptureTextureRenderTarget2D(ComposureProvider->FinalOutputRenderTarget, Options);
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("PixelStreaming set with ComposureRenderTarget"));
		}
		else
		{
			UE_LOG(LogPixelStreamingVCam, Warning, TEXT("PixelStreaming Composure usage was requested, but the specified ComposureOutputProvider has no FinalOutputRenderTarget set"));
		}
	}
	else
	{
		TWeakPtr<FSceneViewport> SceneViewport = GetTargetSceneViewport();
		if (TSharedPtr<FSceneViewport> PinnedSceneViewport = SceneViewport.Pin())
		{
			MediaCapture->CaptureSceneViewport(PinnedSceneViewport, Options);
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("PixelStreaming set to capture scene viewport."));
		}
	}
}

void UVCamPixelStreamingSession::SetupSignallingServer()
{
	// Only start the signalling server if we aren't using an external signalling server
	UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get();
	if (PixelStreamingSubsystem && !IPixelStreamingEditorModule::Get().UseExternalSignallingServer())
	{
		PixelStreamingSubsystem->LaunchSignallingServer();
	}
}

void UVCamPixelStreamingSession::StopSignallingServer()
{
	// Only stop the signalling server if we've been the ones to start it
	UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get();
	if (PixelStreamingSubsystem && !IPixelStreamingEditorModule::Get().UseExternalSignallingServer())
	{
		PixelStreamingSubsystem->StopSignallingServer();
	}
}

void UVCamPixelStreamingSession::Deactivate()
{
	if (UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get())
	{
		PixelStreamingSubsystem->UnregisterActiveOutputProvider(this);
	}

	if (MediaCapture)
	{

		if (MediaOutput && MediaOutput->IsValid())
		{
			// Shutting streamer down before closing signalling server prevents an ugly websocket disconnect showing in the log
			MediaOutput->GetStreamer()->StopStreaming();
		}

		StopSignallingServer();
		MediaCapture->StopCapture(true);
		MediaCapture = nullptr;
	}
	else
	{
		// There is not media capture we defensively clean up the signalling server if it exists.
		StopSignallingServer();
	}

	Super::Deactivate();
	if (bUsingDummyUMG)
	{
		SetUMGClass(nullptr);
		bUsingDummyUMG = false;
	}

	UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	Settings->bThrottleCPUWhenNotForeground = bOldThrottleCPUWhenNotForeground;
	Settings->PostEditChange();
}

void UVCamPixelStreamingSession::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
}

#if WITH_EDITOR
void UVCamPixelStreamingSession::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;
	if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_FromComposureOutputProviderIndex = GET_MEMBER_NAME_CHECKED(UVCamPixelStreamingSession, FromComposureOutputProviderIndex);
		const FName PropertyName = Property->GetFName();
		if (PropertyName == NAME_FromComposureOutputProviderIndex)
		{
			SetActive(false);
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

IMPLEMENT_MODULE(FDefaultModuleImpl, PixelStreamingVCam)
