// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamOutputRemoteSession.h"
#include "VCamOutputComposure.h"
#include "Channels/RemoteSessionImageChannel.h"
#include "Channels/RemoteSessionInputChannel.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameEngine.h"
#include "Widgets/SVirtualWindow.h"
#include "UObject/SoftObjectPath.h"
#include "VPFullScreenUserWidget.h"

#if WITH_EDITOR
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#endif

namespace VCamOutputRemoteSession
{
	static const FName LevelEditorName(TEXT("LevelEditor"));
	static const FSoftClassPath EmptyUMGSoftClassPath(TEXT("/VCamCore/Assets/VCam_EmptyVisibleUMG.VCam_EmptyVisibleUMG_C"));
}

void UVCamOutputRemoteSession::Initialize()
{
	if (!bInitialized && (MediaOutput == nullptr))
	{
		MediaOutput = NewObject<URemoteSessionMediaOutput>(GetTransientPackage(), URemoteSessionMediaOutput::StaticClass());
	}

	Super::Initialize();
}

void UVCamOutputRemoteSession::Deinitialize()
{
	MediaOutput = nullptr;

	Super::Deinitialize();
}

void UVCamOutputRemoteSession::Activate()
{
	// If we don't have a UMG assigned, we still need to create an empty 'dummy' UMG in order to properly route the input back from the RemoteSession device
	if (UMGClass == nullptr)
	{
		bUsingDummyUMG = true;
		UMGClass = VCamOutputRemoteSession::EmptyUMGSoftClassPath.TryLoadClass<UUserWidget>();
	}

	CreateRemoteSession();
	
	Super::Activate();
}

void UVCamOutputRemoteSession::Deactivate()
{
	DestroyRemoteSession();

	Super::Deactivate();

	if (bUsingDummyUMG)
	{
		UMGClass = nullptr;
		bUsingDummyUMG = false;
	}
}

void UVCamOutputRemoteSession::Tick(const float DeltaTime)
{
	if (bIsActive && RemoteSessionHost.IsValid())
	{
		RemoteSessionHost->Tick(DeltaTime);
	}

	Super::Tick(DeltaTime);
}

void UVCamOutputRemoteSession::CreateRemoteSession()
{
	if (!bInitialized)
	{
		UE_LOG(LogVCamOutputProvider, Warning, TEXT("CreateRemoteSession has been called, but has not been initialized yet"));
		return;
	}

	if (!RemoteSessionHost.IsValid())
	{
		if (IRemoteSessionModule* RemoteSession = FModuleManager::LoadModulePtr<IRemoteSessionModule>("RemoteSession"))
		{
			TArray<FRemoteSessionChannelInfo> SupportedChannels;
			SupportedChannels.Emplace(FRemoteSessionInputChannel::StaticType(), ERemoteSessionChannelMode::Read);
			SupportedChannels.Emplace(FRemoteSessionImageChannel::StaticType(), ERemoteSessionChannelMode::Write);

			RemoteSessionHost = RemoteSession->CreateHost(MoveTemp(SupportedChannels), PortNumber);

			if (RemoteSessionHost.IsValid())
			{
				RemoteSessionHost->RegisterChannelChangeDelegate(FOnRemoteSessionChannelChange::FDelegate::CreateUObject(this, &UVCamOutputRemoteSession::OnRemoteSessionChannelChange));

				RemoteSessionHost->Tick(0.0f);

				if (bUseOverrideResolution)
				{
					TSharedPtr<FSceneViewport> SceneViewport = GetTargetSceneViewport();
					if (SceneViewport.IsValid())
					{
						SceneViewport->SetFixedViewportSize(OverrideResolution.X, OverrideResolution.Y);
					}
				}
			}
			else
			{
				UE_LOG(LogVCamOutputProvider, Error, TEXT("Unable to create a RemoteSession host on port %d. Check the port is not in use."), PortNumber);
			}			
		}
	}
	else
	{
		UE_LOG(LogVCamOutputProvider, Warning, TEXT("CreateRemoteSession has been called, but there is already a valid RemoteSessionHost!"));
	}
}

void UVCamOutputRemoteSession::DestroyRemoteSession()
{
	if (RemoteSessionHost.IsValid())
	{
		TSharedPtr<FRemoteSessionInputChannel> InputChannel = RemoteSessionHost->GetChannel<FRemoteSessionInputChannel>();
		if (InputChannel)
		{
			InputChannel->GetOnRouteTouchDownToWidgetFailedDelegate()->RemoveAll(this);
		}

		RemoteSessionHost->Close(TEXT("Camera Shutdown"));
		RemoteSessionHost.Reset();
		if (MediaCapture)
		{
			MediaCapture->StopCapture(true);
			MediaCapture = nullptr;
		}

		// If everything is being shut down somehow, get out now
		if (GEngine == nullptr)
		{
			return;
		}

#if WITH_EDITOR
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Editor)
			{
				if (FLevelEditorViewportClient* LevelViewportClient = GetTargetLevelViewportClient())
				{
					LevelViewportClient->ViewFOV = LevelViewportClient->FOVAngle;
					GEditor->RemovePerspectiveViewRotation(true, true, false);
				}
			}
		}
#endif

		if (bUseOverrideResolution)
		{
			TSharedPtr<FSceneViewport> SceneViewport = GetTargetSceneViewport();
			if (SceneViewport.IsValid())
			{
				SceneViewport->SetFixedViewportSize(0, 0);
			}
		}
	}
}

void UVCamOutputRemoteSession::OnRemoteSessionChannelChange(IRemoteSessionRole* Role, TWeakPtr<IRemoteSessionChannel> Channel, ERemoteSessionChannelChange Change)
{
	TSharedPtr<IRemoteSessionChannel> PinnedChannel = Channel.Pin();
	if (PinnedChannel && Change == ERemoteSessionChannelChange::Created)
	{
        const FString PinnedChannelTypeString(PinnedChannel->GetType());
        
		if (PinnedChannelTypeString.Compare(FRemoteSessionImageChannel::StaticType()) == 0)
		{
			OnImageChannelCreated(Channel);
		}
        else if (PinnedChannelTypeString.Compare(FRemoteSessionInputChannel::StaticType()) == 0)
		{
			OnInputChannelCreated(Channel);
		}
	}
}

void UVCamOutputRemoteSession::OnImageChannelCreated(TWeakPtr<IRemoteSessionChannel> Instance)
{
	TSharedPtr<FRemoteSessionImageChannel> ImageChannel = StaticCastSharedPtr<FRemoteSessionImageChannel>(Instance.Pin());
	if (ImageChannel)
	{
		ImageChannel->SetImageProvider(nullptr);
		MediaOutput->SetImageChannel(ImageChannel);
		MediaCapture = Cast<URemoteSessionMediaCapture>(MediaOutput->CreateMediaCapture());

		FMediaCaptureOptions Options;
		Options.bResizeSourceBuffer = true;

		// If we are rendering from a ComposureOutputProvider, get the requested render target and use that instead of the viewport
		if (UVCamOutputComposure* ComposureProvider = Cast<UVCamOutputComposure>(GetOtherOutputProviderByIndex(FromComposureOutputProviderIndex)))
		{
			if (ComposureProvider->FinalOutputRenderTarget)
			{
				MediaCapture->CaptureTextureRenderTarget2D(ComposureProvider->FinalOutputRenderTarget, Options);
				UE_LOG(LogVCamOutputProvider, Log, TEXT("ImageChannel callback - MediaCapture set with ComposureRenderTarget"));
			}
			else
			{
				UE_LOG(LogVCamOutputProvider, Warning, TEXT("ImageChannel callback - Composure usage was requested, but the specified ComposureOutputProvider has no FinalOutputRenderTarget set"));
			}
		}
		else
		{
			TWeakPtr<FSceneViewport> SceneViewport = GetTargetSceneViewport();
			if (TSharedPtr<FSceneViewport> PinnedSceneViewport = SceneViewport.Pin())
			{
				MediaCapture->CaptureSceneViewport(PinnedSceneViewport, Options);
				UE_LOG(LogVCamOutputProvider, Log, TEXT("ImageChannel callback - MediaCapture set with viewport"));
			}
		}
	}
}

void UVCamOutputRemoteSession::OnInputChannelCreated(TWeakPtr<IRemoteSessionChannel> Instance)
{
	TSharedPtr<FRemoteSessionInputChannel> InputChannel = StaticCastSharedPtr<FRemoteSessionInputChannel>(Instance.Pin());
	if (InputChannel)
	{
		// If we have a UMG, then use it
		if (UMGClass && UMGWidget)
		{
			TSharedPtr<SVirtualWindow> InputWindow;

			// If we are rendering from a ComposureOutputProvider, we need to get the InputWindow from that UMG, not the one in the RemoteSessionOutputProvider
			if (UVCamOutputComposure* ComposureProvider = Cast<UVCamOutputComposure>(GetOtherOutputProviderByIndex(FromComposureOutputProviderIndex)))
			{
				if (UVPFullScreenUserWidget* ComposureUMGWidget = ComposureProvider->GetUMGWidget())
				{
					InputWindow = ComposureUMGWidget->PostProcessDisplayType.GetSlateWindow();
					UE_LOG(LogVCamOutputProvider, Log, TEXT("InputChannel callback - Routing input to active viewport with Composure UMG"));
				}
				else
				{
					UE_LOG(LogVCamOutputProvider, Warning, TEXT("InputChannel callback - Composure usage was requested, but the specified ComposureOutputProvider has no UMG set"));
				}
			}
			else
			{
				InputWindow = UMGWidget->PostProcessDisplayType.GetSlateWindow();
				UE_LOG(LogVCamOutputProvider, Log, TEXT("InputChannel callback - Routing input to active viewport with UMG"));
			}

			InputChannel->SetPlaybackWindow(InputWindow, nullptr);
			InputChannel->TryRouteTouchMessageToWidget(true);
			InputChannel->GetOnRouteTouchDownToWidgetFailedDelegate()->AddUObject(this, &UVCamOutputRemoteSession::OnTouchEventOutsideUMG);
		}
		else
		{
			InputChannel->SetPlaybackWindow(GetTargetInputWindow(), nullptr);
			InputChannel->TryRouteTouchMessageToWidget(true);
			InputChannel->GetOnRouteTouchDownToWidgetFailedDelegate()->AddUObject(this, &UVCamOutputRemoteSession::OnTouchEventOutsideUMG);

			UE_LOG(LogVCamOutputProvider, Log, TEXT("InputChannel callback - Routing input to active viewport"));
		}
	}
}

void UVCamOutputRemoteSession::OnTouchEventOutsideUMG(const FVector2D& InViewportPosition)
{
	const float MaxFocusTraceDistance = 1000000.0f;
	FVector TraceDirection;
	FVector CameraWorldLocation;

	if (!DeprojectScreenToWorld(InViewportPosition, CameraWorldLocation, TraceDirection))
	{
		return;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World)
		{
			FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(UpdateAutoFocus), true);
			const FVector TraceEnd = CameraWorldLocation + TraceDirection * MaxFocusTraceDistance;
			const bool bHit = World->LineTraceSingleByChannel(LastViewportTouchResult, CameraWorldLocation, TraceEnd, ECC_Visibility, TraceParams);
			if (bHit)
			{
#if WITH_EDITOR
				// @todo: This doesn't seem like the most efficient way to pass click events to the editor viewport...
				if (Context.WorldType == EWorldType::Editor)
				{
					if (FLevelEditorViewportClient* LevelViewportClient = GetTargetLevelViewportClient())
					{
						if (FViewport* ActiveViewport = LevelViewportClient->Viewport)
						{
							FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
								ActiveViewport,
								LevelViewportClient->GetScene(),
								LevelViewportClient->EngineShowFlags)
								.SetRealtimeUpdate(true));
							FSceneView* View = LevelViewportClient->CalcSceneView(&ViewFamily);

							uint32 HitX = InViewportPosition.X;
							uint32 HitY = InViewportPosition.Y;
							HHitProxy* HitProxy = ActiveViewport->GetHitProxy(HitX, HitY);
							LevelViewportClient->ProcessClick(*View, HitProxy, EKeys::LeftMouseButton, EInputEvent::IE_Pressed, HitX, HitY);
						}
					}
				}
				else
#endif
				{
					LastViewportTouchResult.GetHitObjectHandle().FetchActor()->NotifyActorOnClicked();
				}
			}
		}
	}
}

bool UVCamOutputRemoteSession::DeprojectScreenToWorld(const FVector2D& InScreenPosition, FVector& OutWorldPosition, FVector& OutWorldDirection) const
{
	bool bSuccess = false;

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
		{
			APlayerController* PC = Context.OwningGameInstance->GetFirstLocalPlayerController(Context.World());
			if (PC)
			{
				bSuccess |= PC->DeprojectScreenPositionToWorld(InScreenPosition.X, InScreenPosition.Y, OutWorldPosition, OutWorldDirection);
				break;
			}
		}
#if WITH_EDITOR
		else if (Context.WorldType == EWorldType::Editor)
		{
			if (FLevelEditorViewportClient* LevelViewportClient = GetTargetLevelViewportClient())
			{
				if (FViewport* ActiveViewport = LevelViewportClient->Viewport)
				{
					FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
						ActiveViewport,
						LevelViewportClient->GetScene(),
						LevelViewportClient->EngineShowFlags)
						.SetRealtimeUpdate(true));
					FSceneView* View = LevelViewportClient->CalcSceneView(&ViewFamily);

					const FIntPoint ViewportSize = ActiveViewport->GetSizeXY();
					const FIntRect ViewRect = FIntRect(0, 0, ViewportSize.X, ViewportSize.Y);
					const FMatrix InvViewProjectionMatrix = View->ViewMatrices.GetInvViewProjectionMatrix();
					FSceneView::DeprojectScreenToWorld(InScreenPosition, ViewRect, InvViewProjectionMatrix, OutWorldPosition, OutWorldDirection);
					bSuccess = true;
				}
			}
		}
#endif
	}

	if (!bSuccess)
	{
		OutWorldPosition = FVector::ZeroVector;
		OutWorldDirection = FVector::ZeroVector;
	}
	return bSuccess;
}

#if WITH_EDITOR
void UVCamOutputRemoteSession::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_PortNumber = GET_MEMBER_NAME_CHECKED(UVCamOutputRemoteSession, PortNumber);
		static FName NAME_FromComposureOutputProviderIndex = GET_MEMBER_NAME_CHECKED(UVCamOutputRemoteSession, FromComposureOutputProviderIndex);

		if ((Property->GetFName() == NAME_PortNumber) ||
			(Property->GetFName() == NAME_FromComposureOutputProviderIndex))
		{
			if (bIsActive)
			{
				SetActive(false);
				SetActive(true);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
