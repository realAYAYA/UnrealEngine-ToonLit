// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamPixelStreamingSessionLogic.h"

#include "BuiltinProviders/VCamPixelStreamingSession.h"
#include "Output/VCamOutputComposure.h"
#include "VCamComponent.h"
#include "VCamPixelStreamingLiveLink.h"
#include "VCamPixelStreamingSubsystem.h"

#include "Async/Async.h"
#include "Containers/UnrealString.h"
#include "Editor/EditorPerformanceSettings.h"
#include "IPixelStreamingModule.h"
#include "IPixelStreamingInputModule.h"
#include "IPixelStreamingEditorModule.h"
#include "PixelStreamingInputEnums.h"
#include "PixelStreamingInputMessage.h"
#include "PixelStreamingInputProtocol.h"
#include "PixelStreamingMediaOutput.h"
#include "PixelStreamingServers.h"
#include "PixelStreamingVCamLog.h"
#include "Math/Matrix.h"
#include "Serialization/MemoryReader.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/VPFullScreenUserWidget.h"

namespace UE::PixelStreamingVCam::Private
{
	int32 FVCamPixelStreamingSessionLogic::NextDefaultStreamerId = 1;
	
	void FVCamPixelStreamingSessionLogic::OnDeinitialize(DecoupledOutputProvider::IOutputProviderEvent& Args)
	{
		if (MediaOutput)
		{
			MediaOutput->ConditionalBeginDestroy();
			MediaOutput = nullptr;
		}
	}

	void FVCamPixelStreamingSessionLogic::OnActivate(DecoupledOutputProvider::IOutputProviderEvent& Args)
	{
		UVCamPixelStreamingSession* This = Cast<UVCamPixelStreamingSession>(&Args.GetOutputProvider());
		const TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr = This;
		
		if (!This->IsInitialized())
		{
			UE_LOG(LogPixelStreamingVCam, Warning, TEXT("Trying to start Pixel Streaming, but has not been initialized yet"));
			This->SetActive(false);
			return;
		}

		if (This->StreamerId.IsEmpty())
		{
			This->StreamerId = FString::Printf(TEXT("VCam%d"), NextDefaultStreamerId++);
		}

		// Rename the underlying session to the streamer name
		This->Rename(*This->StreamerId);

		// Setup livelink source
		UVCamPixelStreamingSubsystem::Get()->TryGetLiveLinkSource(This);
		if (UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get())
		{
			PixelStreamingSubsystem->RegisterActiveOutputProvider(This);
			ConditionallySetLiveLinkSubjectToThis(This);
		}

		// If we don't have a UMG assigned, we still need to create an empty 'dummy' UMG in order to properly route the input back from the RemoteSession device
		if (!This->GetUMGClass())
		{
			bUsingDummyUMG = true;
			const FSoftClassPath EmptyUMGSoftClassPath(TEXT("/VCamCore/Assets/VCam_EmptyVisibleUMG.VCam_EmptyVisibleUMG_C"));
			This->SetUMGClass(EmptyUMGSoftClassPath.TryLoadClass<UUserWidget>());
		}

		// create a new media output if we dont already have one, or its not valid, or if the id has changed
		if (MediaOutput == nullptr || !MediaOutput->IsValid() || MediaOutput->GetStreamer()->GetId() != This->StreamerId)
		{
			MediaOutput = UPixelStreamingMediaOutput::Create(GetTransientPackage(), This->StreamerId);
			MediaOutput->OnRemoteResolutionChanged().AddSP(this, &FVCamPixelStreamingSessionLogic::OnRemoteResolutionChanged, WeakThisPtr);
		}

		UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
		bOldThrottleCPUWhenNotForeground = Settings->bThrottleCPUWhenNotForeground;
		if (This->PreventEditorIdle)
		{
			Settings->bThrottleCPUWhenNotForeground = false;
			Settings->PostEditChange();
		}

		// This sets up media capture and streamer
		SetupCapture(WeakThisPtr);

		// Super::Activate() creates our UMG which we need before setting up our custom input handling
		Args.ExecuteSuperFunction();

		// We setup custom handling of ARKit transforms coming from iOS devices here
		SetupCustomInputHandling(This);
		// We need signalling server to be up before we can start streaming
		SetupSignallingServer();

		if (MediaOutput->IsValid())
		{
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("Activating PixelStreaming VCam Session. Endpoint: %s"), *MediaOutput->GetStreamer()->GetSignallingServerURL());
		}
	}

	void FVCamPixelStreamingSessionLogic::OnDeactivate(DecoupledOutputProvider::IOutputProviderEvent& Args)
	{
		UVCamPixelStreamingSession* This = Cast<UVCamPixelStreamingSession>(&Args.GetOutputProvider());
		if (UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get())
		{
			PixelStreamingSubsystem->UnregisterActiveOutputProvider(This);
		}

		if (MediaCapture)
		{

			if (MediaOutput && MediaOutput->IsValid())
			{
				// Shutting streamer down before closing signalling server prevents an ugly websocket disconnect showing in the log
				MediaOutput->GetStreamer()->StopStreaming();
			}

			StopSignallingServer();
			MediaCapture->StopCapture(false);
			MediaCapture = nullptr;
		}
		else
		{
			// There is not media capture we defensively clean up the signalling server if it exists.
			StopSignallingServer();
		}

		Args.ExecuteSuperFunction();
		if (bUsingDummyUMG)
		{
			This->SetUMGClass(nullptr);
			bUsingDummyUMG = false;
		}

		UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
		Settings->bThrottleCPUWhenNotForeground = bOldThrottleCPUWhenNotForeground;
		Settings->PostEditChange();
	}

	void FVCamPixelStreamingSessionLogic::OnAddReferencedObjects(DecoupledOutputProvider::IOutputProviderEvent& Args, FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(MediaOutput, &Args.GetOutputProvider());
		Collector.AddReferencedObject(MediaCapture, &Args.GetOutputProvider());
	}

#if WITH_EDITOR
	void FVCamPixelStreamingSessionLogic::OnPostEditChangeProperty(DecoupledOutputProvider::IOutputProviderEvent& Args, FPropertyChangedEvent& PropertyChangedEvent)
	{
		UVCamPixelStreamingSession* This = Cast<UVCamPixelStreamingSession>(&Args.GetOutputProvider());
		
		FProperty* Property = PropertyChangedEvent.MemberProperty;
		if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
		{
			const FName PropertyName = Property->GetFName();
			if (PropertyName == GET_MEMBER_NAME_CHECKED(UVCamPixelStreamingSession, FromComposureOutputProviderIndex))
			{
				This->SetActive(false);
			}
			else if (PropertyName == GET_MEMBER_NAME_CHECKED(UVCamPixelStreamingSession, bAutoSetLiveLinkSubject))
			{
				ConditionallySetLiveLinkSubjectToThis(This);
			}
		}
	}
#endif

	void FVCamPixelStreamingSessionLogic::SetupSignallingServer()
	{
		// Only start the signalling server if we aren't using an external signalling server
		UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get();
		if (PixelStreamingSubsystem && !IPixelStreamingEditorModule::Get().UseExternalSignallingServer())
		{
			PixelStreamingSubsystem->LaunchSignallingServer();
		}
	}

	void FVCamPixelStreamingSessionLogic::StopSignallingServer()
	{
		// Only stop the signalling server if we've been the ones to start it
		UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get();
		if (PixelStreamingSubsystem && !IPixelStreamingEditorModule::Get().UseExternalSignallingServer())
		{
			PixelStreamingSubsystem->StopSignallingServer();
		}
	}

	void FVCamPixelStreamingSessionLogic::SetupCapture(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr)
	{
		UE_LOG(LogPixelStreamingVCam, Log, TEXT("Create new media capture for Pixel Streaming VCam."));

		if (MediaCapture)
		{
			MediaCapture->OnStateChangedNative.RemoveAll(this);
		}

		// Create a capturer that will capture frames from viewport and send them to streamer
		MediaCapture = Cast<UPixelStreamingMediaCapture>(MediaOutput->CreateMediaCapture());
		MediaCapture->OnStateChangedNative.AddSP(this, &FVCamPixelStreamingSessionLogic::OnCaptureStateChanged, WeakThisPtr);
		StartCapture(WeakThisPtr);
	}

	void FVCamPixelStreamingSessionLogic::StartCapture(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr)
	{
		if (!ensure(WeakThisPtr.IsValid()) || !MediaCapture)
		{
			return;
		}

		FMediaCaptureOptions Options;
		Options.OverrunAction = EMediaCaptureOverrunAction::Skip;
		Options.ResizeMethod = EMediaCaptureResizeMethod::ResizeSource;

		// If we are rendering from a ComposureOutputProvider, get the requested render target and use that instead of the viewport
		if (UVCamOutputComposure* ComposureProvider = Cast<UVCamOutputComposure>(WeakThisPtr->GetOtherOutputProviderByIndex(WeakThisPtr->FromComposureOutputProviderIndex)))
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
			TWeakPtr<FSceneViewport> SceneViewport = WeakThisPtr->GetTargetSceneViewport();
			if (TSharedPtr<FSceneViewport> PinnedSceneViewport = SceneViewport.Pin())
			{
				MediaCapture->CaptureSceneViewport(PinnedSceneViewport, Options);
				UE_LOG(LogPixelStreamingVCam, Log, TEXT("PixelStreaming set to capture scene viewport."));
			}
		}
	}

	void FVCamPixelStreamingSessionLogic::SetupCustomInputHandling(UVCamPixelStreamingSession* This)
	{
		if (This->GetUMGWidget())
		{
			TSharedPtr<SVirtualWindow> InputWindow;
			// If we are rendering from a ComposureOutputProvider, we need to get the InputWindow from that UMG, not the one in the PixelStreamingOutputProvider
			if (UVCamOutputComposure* ComposureProvider = Cast<UVCamOutputComposure>(This->GetOtherOutputProviderByIndex(This->FromComposureOutputProviderIndex)))
			{
				if (UVPFullScreenUserWidget* ComposureUMGWidget = ComposureProvider->GetUMGWidget())
				{
					const EVPWidgetDisplayType WidgetDisplayType = ComposureUMGWidget->GetDisplayType(This->GetWorld());
					if (ensure(UVPFullScreenUserWidget::DoesDisplayTypeUsePostProcessSettings(WidgetDisplayType)))
					{
						InputWindow = ComposureUMGWidget->GetPostProcessDisplayTypeSettingsFor(WidgetDisplayType)->GetSlateWindow();
						UE_LOG(LogPixelStreamingVCam, Log, TEXT("InputChannel callback - Routing input to active viewport with Composure UMG"));
					}
				}
				else
				{
					UE_LOG(LogPixelStreamingVCam, Warning, TEXT("InputChannel callback - Composure usage was requested, but the specified ComposureOutputProvider has no UMG set"));
				}
			}
			else
			{
				checkf(UVPFullScreenUserWidget::DoesDisplayTypeUsePostProcessSettings(EVPWidgetDisplayType::PostProcessSceneViewExtension), TEXT("DisplayType not set up correctly in constructor!"));
				InputWindow = This->GetUMGWidget()->GetPostProcessDisplayTypeSettingsFor(EVPWidgetDisplayType::PostProcessSceneViewExtension)->GetSlateWindow();
				UE_LOG(LogPixelStreamingVCam, Log, TEXT("InputChannel callback - Routing input to active viewport with UMG"));
			}

			MediaOutput->GetStreamer()->SetTargetWindow(InputWindow);
			MediaOutput->GetStreamer()->SetInputHandlerType(EPixelStreamingInputType::RouteToWidget);
		}
		else
		{
			MediaOutput->GetStreamer()->SetTargetWindow(This->GetTargetInputWindow());
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

			const TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr = This;
			const TFunction<void(FMemoryReader)> ARKitHandler = [WeakThisPtr](FMemoryReader Ar)
			{
				if (!WeakThisPtr.IsValid() || !WeakThisPtr->EnableARKitTracking)
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

				if (const TSharedPtr<FPixelStreamingLiveLinkSource> LiveLinkSource = UVCamPixelStreamingSubsystem::Get()->TryGetLiveLinkSource(WeakThisPtr.Get()))
				{
					LiveLinkSource->PushTransformForSubject(WeakThisPtr->GetFName(), FTransform(ARKitMatrix), Timestamp);
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

	void FVCamPixelStreamingSessionLogic::OnCaptureStateChanged(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr)
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
				SetupCapture(WeakThisPtr);
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

	void FVCamPixelStreamingSessionLogic::OnRemoteResolutionChanged(const FIntPoint& RemoteResolution, TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr)
	{
		// Early out if match remote resolution is not enabled.
		if (!ensure(WeakThisPtr.IsValid()) || !WeakThisPtr->bMatchRemoteResolution)
		{
			return;
		}

		// Ensure override resolution is being used
		if (!WeakThisPtr->bUseOverrideResolution)
		{
			WeakThisPtr->bUseOverrideResolution = true;
		}

		// Set the override resolution on the output provider base, this will trigger a resize
		WeakThisPtr->OverrideResolution = RemoteResolution;
		WeakThisPtr->ReapplyOverrideResolution();
	}

	void FVCamPixelStreamingSessionLogic::ConditionallySetLiveLinkSubjectToThis(UVCamPixelStreamingSession* This) const
	{
		UVCamComponent* VCamComponent = This->GetTypedOuter<UVCamComponent>();
		if (This->bAutoSetLiveLinkSubject && IsValid(VCamComponent) && This->IsActive())
		{
			VCamComponent->SetLiveLinkSubobject(This->GetFName());
		}
	}
}
