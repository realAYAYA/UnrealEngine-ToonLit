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
#include "IPixelStreamingStats.h"
#include "IPixelStreamingModule.h"
#include "IPixelStreamingInputModule.h"
#include "IPixelStreamingEditorModule.h"
#include "PixelStreamingInputEnums.h"
#include "PixelStreamingInputMessage.h"
#include "PixelStreamingInputProtocol.h"
#include "PixelStreamingMediaOutput.h"
#include "PixelStreamingServers.h"
#include "PixelStreamingVCamLog.h"
#include "PixelStreamingVCamModule.h"
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
		const TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr = This;

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
			MediaOutput->OnRemoteResolutionChanged().AddSP(this, &FVCamPixelStreamingSessionLogic::OnRemoteResolutionChanged, WeakThisUObjectPtr);
			MediaOutput->GetStreamer()->OnPreConnection().AddSP(this, &FVCamPixelStreamingSessionLogic::OnPreStreaming, WeakThisUObjectPtr);
			MediaOutput->GetStreamer()->OnStreamingStarted().AddSP(this, &FVCamPixelStreamingSessionLogic::OnStreamingStarted, WeakThisUObjectPtr);
			MediaOutput->GetStreamer()->OnStreamingStopped().AddSP(this, &FVCamPixelStreamingSessionLogic::OnStreamingStopped);
		}

		UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
		bOldThrottleCPUWhenNotForeground = Settings->bThrottleCPUWhenNotForeground;
		if (This->PreventEditorIdle)
		{
			Settings->bThrottleCPUWhenNotForeground = false;
			Settings->PostEditChange();
		}

		// Super::Activate() creates our UMG which we need before setting up our custom input handling
		Args.ExecuteSuperFunction();

		// We setup custom handling of ARKit transforms coming from iOS devices here
		SetupCustomInputHandling(This);
		// We need signalling server to be up before we can start streaming
		SetupSignallingServer();

		if (MediaOutput)
		{
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("Activating PixelStreaming VCam Session. Endpoint: %s"), *MediaOutput->GetStreamer()->GetSignallingServerURL());

			// Start streaming here, this will trigger capturer to start
			MediaOutput->StartStreaming();
		}

		FPixelStreamingVCamModule::Get().AddActiveSession(WeakThisUObjectPtr);
	}

	void FVCamPixelStreamingSessionLogic::OnDeactivate(DecoupledOutputProvider::IOutputProviderEvent& Args)
	{
		UVCamPixelStreamingSession* This = Cast<UVCamPixelStreamingSession>(&Args.GetOutputProvider());
		if (UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get())
		{
			PixelStreamingSubsystem->UnregisterActiveOutputProvider(This);
		}

		StopEverything();

		Args.ExecuteSuperFunction();
		if (bUsingDummyUMG)
		{
			This->SetUMGClass(nullptr);
			bUsingDummyUMG = false;
		}

		UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
		Settings->bThrottleCPUWhenNotForeground = bOldThrottleCPUWhenNotForeground;
		Settings->PostEditChange();

		const TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr = This;
		FPixelStreamingVCamModule::Get().RemoveActiveSession(WeakThisPtr);
	}

	VCamCore::EViewportChangeReply FVCamPixelStreamingSessionLogic::PreReapplyViewport(DecoupledOutputProvider::IOutputProviderEvent& Args)
	{
		return VCamCore::EViewportChangeReply::ApplyViewportChange;
	}

	void FVCamPixelStreamingSessionLogic::PostReapplyViewport(DecoupledOutputProvider::IOutputProviderEvent& Args)
	{
		StopCapture();
		
		UVCamPixelStreamingSession* This = Cast<UVCamPixelStreamingSession>(&Args.GetOutputProvider());
		check(This);
		
		SetupCapture(This);
		SetupCustomInputHandling(This);
	}

	void FVCamPixelStreamingSessionLogic::StopCapture()
	{
		if (MediaCapture)
		{
			MediaCapture->StopCapture(false);
			MediaCapture = nullptr;
		}
	}

	void FVCamPixelStreamingSessionLogic::OnPreStreaming(IPixelStreamingStreamer* PreConnectionStreamer, TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr)
	{
		SetupCapture(WeakThisUObjectPtr);
	}

	void FVCamPixelStreamingSessionLogic::StopStreaming()
	{
		if(!MediaOutput)
		{
			return;
		}

		MediaOutput->StopStreaming();
	}

	void FVCamPixelStreamingSessionLogic::OnStreamingStarted(IPixelStreamingStreamer* StartedStreamer, TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr)
	{
		SetupARKitResponseTimer(WeakThisUObjectPtr);
	}

	void FVCamPixelStreamingSessionLogic::OnStreamingStopped(IPixelStreamingStreamer* StartedStreamer)
	{
		StopARKitResponseTimer();
		StopCapture();
	}

	void FVCamPixelStreamingSessionLogic::StopEverything()
	{
		StopStreaming();
		StopSignallingServer();
		StopCapture();
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
		IPixelStreamingEditorModule& PSEditorModule = IPixelStreamingEditorModule::Get();

		if(PSEditorModule.UseExternalSignallingServer())
		{
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("VCam cannot stop an `external` signalling server from UE - skipping stopping signalling server."));
			return;
		}

		TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = PSEditorModule.GetSignallingServer();

		if(!SignallingServer)
		{
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("VCam cannot stop internal signalling server because it is already null - skipping stopping signalling server."));
			return;
		}

		// Asynchronously get the number of streamers and if it we have more than just this connect do not shut down the SS it might be used by something else
		SignallingServer->GetNumStreamers([](uint16 NumStreamers){
			if(NumStreamers > 1)
			{
				UE_LOG(LogPixelStreamingVCam, Log, TEXT("VCam cannot shutdown internal signalling server because there are still multiple streamers connected."));
				return;
			}

			// Only stop the signalling server if we've been the ones to start it
			UVCamPixelStreamingSubsystem* PixelStreamingSubsystem = UVCamPixelStreamingSubsystem::Get();

			if(!PixelStreamingSubsystem)
			{
				return;
			}

			PixelStreamingSubsystem->StopSignallingServer();
		});
	}

	void FVCamPixelStreamingSessionLogic::SetupCapture(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr)
	{
		UE_LOG(LogPixelStreamingVCam, Log, TEXT("Create new media capture for Pixel Streaming VCam."));

		if (MediaCapture)
		{
			MediaCapture->OnStateChangedNative.RemoveAll(this);
		}

		// Create a capturer that will capture frames from viewport and send them to streamer
		MediaCapture = Cast<UPixelStreamingMediaIOCapture>(MediaOutput->CreateMediaCapture());
		MediaCapture->OnStateChangedNative.AddSP(this, &FVCamPixelStreamingSessionLogic::OnCaptureStateChanged, WeakThisUObjectPtr);
		StartCapture(WeakThisUObjectPtr);
	}

	void FVCamPixelStreamingSessionLogic::StartCapture(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr)
	{
		if (!WeakThisUObjectPtr.IsValid() || !MediaCapture)
		{
			return;
		}

		FMediaCaptureOptions Options;
		Options.bSkipFrameWhenRunningExpensiveTasks = false;
		Options.OverrunAction = EMediaCaptureOverrunAction::Skip;
		Options.ResizeMethod = EMediaCaptureResizeMethod::ResizeSource;

		// If we are rendering from a ComposureOutputProvider, get the requested render target and use that instead of the viewport
		if (UVCamOutputComposure* ComposureProvider = Cast<UVCamOutputComposure>(WeakThisUObjectPtr->GetOtherOutputProviderByIndex(WeakThisUObjectPtr->FromComposureOutputProviderIndex)))
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
			TWeakPtr<FSceneViewport> SceneViewport = WeakThisUObjectPtr->GetTargetSceneViewport();
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

			const TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr = This;
			const IPixelStreamingInputHandler::MessageHandlerFn ARKitHandler = [this, WeakThisUObjectPtr](FString PlayerId, FMemoryReader Ar)
			{
				NumARKitEvents++;

				if (!WeakThisUObjectPtr.IsValid() || !WeakThisUObjectPtr->EnableARKitTracking)
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

				if (const TSharedPtr<FPixelStreamingLiveLinkSource> LiveLinkSource = UVCamPixelStreamingSubsystem::Get()->TryGetLiveLinkSource(WeakThisUObjectPtr.Get()))
				{
					LiveLinkSource->PushTransformForSubject(FName(WeakThisUObjectPtr->StreamerId), FTransform(ARKitMatrix), Timestamp);
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

	void FVCamPixelStreamingSessionLogic::OnCaptureStateChanged(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr)
	{
		if (!MediaCapture)
		{
			return;
		}

		switch (MediaCapture->GetState())
		{
		case EMediaCaptureState::Capturing:
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("Starting media capture for Pixel Streaming VCam."));
			break;
		case EMediaCaptureState::Stopped:
			if (MediaCapture->WasViewportResized())
			{
				UE_LOG(LogPixelStreamingVCam, Log, TEXT("Pixel Streaming VCam capture was stopped due to resize, going to restart capture."));
				// If it was stopped and viewport resized we assume resize caused the stop, so try a restart of capture here.
				SetupCapture(WeakThisUObjectPtr);
			}
			else
			{
				UE_LOG(LogPixelStreamingVCam, Log, TEXT("Stopping media capture for Pixel Streaming VCam."));
			}
			break;
		case EMediaCaptureState::Error:
			UE_LOG(LogPixelStreamingVCam, Log, TEXT("Pixel Streaming VCam capture hit an error, capturing will stop."));
			break;
		default:
			break;
		}
	}

	void FVCamPixelStreamingSessionLogic::OnRemoteResolutionChanged(const FIntPoint& RemoteResolution, TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr)
	{
		// Early out if match remote resolution is not enabled.
		if (!ensure(WeakThisUObjectPtr.IsValid()) || !WeakThisUObjectPtr->bMatchRemoteResolution)
		{
			return;
		}

		// No need to apply override resolution if resolutions are the same (i.e. there was no actual resolution change).
		if(WeakThisUObjectPtr->OverrideResolution == RemoteResolution)
		{
			return;
		}

		// Ensure override resolution is being used
		if (!WeakThisUObjectPtr->bUseOverrideResolution)
		{
			WeakThisUObjectPtr->bUseOverrideResolution = true;
		}

		// Set the override resolution on the output provider base, this will trigger a resize
		WeakThisUObjectPtr->OverrideResolution = RemoteResolution;
		WeakThisUObjectPtr->ReapplyOverrideResolution();
	}

	void FVCamPixelStreamingSessionLogic::ConditionallySetLiveLinkSubjectToThis(UVCamPixelStreamingSession* This) const
	{
		UVCamComponent* VCamComponent = This->GetTypedOuter<UVCamComponent>();
		if (This->bAutoSetLiveLinkSubject && IsValid(VCamComponent) && This->IsActive())
		{
			VCamComponent->SetLiveLinkSubobject(FName(This->StreamerId));
		}
	}

	void FVCamPixelStreamingSessionLogic::SetupARKitResponseTimer(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr)
	{
		if (GWorld && !GWorld->GetTimerManager().IsTimerActive(ARKitResponseTimer))
		{
			const auto SendARKitResponseFunction = [this, WeakThisUObjectPtr]() {
				if(!MediaOutput || !WeakThisUObjectPtr.IsValid())
				{
					return;
				}

				MediaOutput->GetStreamer()->SendPlayerMessage(FPixelStreamingInputProtocol::FromStreamerProtocol.Find("Response")->GetID(), FString::FromInt((int)NumARKitEvents));

				FName GraphName = FName(*(FString(TEXT("NTransformsSentSec_")) + WeakThisUObjectPtr->GetFName().ToString()));
				IPixelStreamingStats::Get().GraphValue(GraphName, NumARKitEvents, 60, 0, 300);
				NumARKitEvents = 0;
			};

			GWorld->GetTimerManager().SetTimer(ARKitResponseTimer, SendARKitResponseFunction, 1.0f, true);
		}
	}

	void FVCamPixelStreamingSessionLogic::StopARKitResponseTimer()
	{
		if (GWorld)
		{
			GWorld->GetTimerManager().ClearTimer(ARKitResponseTimer);
		}
	}
}
