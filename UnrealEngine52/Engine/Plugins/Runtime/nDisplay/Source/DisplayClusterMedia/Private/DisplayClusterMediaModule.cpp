// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterMediaModule.h"
#include "DisplayClusterMediaLog.h"
#include "DisplayClusterMediaHelpers.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "DisplayClusterEnums.h"
#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Cluster/IDisplayClusterClusterManager.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Capture/DisplayClusterMediaCaptureCamera.h"
#include "Capture/DisplayClusterMediaCaptureNode.h"
#include "Capture/DisplayClusterMediaCaptureViewport.h"
#include "Input/DisplayClusterMediaInputNode.h"
#include "Input/DisplayClusterMediaInputViewport.h"
#include "Input/SharedMemoryMediaPlayerFactory.h"

#include "IMediaModule.h"
#include "Misc/CoreDelegates.h"


static TAutoConsoleVariable<int32> CVarMediaEnabled(
	TEXT("nDisplay.media.Enabled"),
	1,
	TEXT("nDisplay media subsystem\n")
	TEXT("0 : Disabled\n")
	TEXT("1 : Enabled\n")
	,
	ECVF_ReadOnly
);


void FDisplayClusterMediaModule::StartupModule()
{
	UE_LOG(LogDisplayClusterMedia, Log, TEXT("Starting module 'DisplayClusterMedia'..."));

	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterCustomPresentSet().AddRaw(this, &FDisplayClusterMediaModule::OnCustomPresentSet);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FDisplayClusterMediaModule::OnEnginePreExit);

	// Create Player Factories
	PlayerFactories.Add(MakeUnique<FSharedMemoryMediaPlayerFactory>());


	// register share memory player factory
	if (IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media"))
	{
		for (TUniquePtr<IMediaPlayerFactory>& Factory : PlayerFactories)
		{
			MediaModule->RegisterPlayerFactory(*Factory);
		}
	}
}

void FDisplayClusterMediaModule::ShutdownModule()
{
	UE_LOG(LogDisplayClusterMedia, Log, TEXT("Shutting down module 'DisplayClusterMedia'..."));

	// unregister share memory player factory
	if (IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media"))
	{
		for (TUniquePtr<IMediaPlayerFactory>& Factory : PlayerFactories)
		{
			MediaModule->UnregisterPlayerFactory(*Factory);
		}
	}

	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterCustomPresentSet().RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
}

void FDisplayClusterMediaModule::OnCustomPresentSet()
{
	// We initialize and start media when backbuffer is available. This CustomPresentSet event is a
	// sign that the backbuffer is already available. This allows us to prevent any potential problems
	// if any of the following is used under the hood, or will be used in the future:
	// - UMediaCapture::CaptureActiveSceneViewport
	// - UMediaCapture::CaptureSceneViewport
	InitializeMedia();
	StartCapture();
	PlayMedia();
}

void FDisplayClusterMediaModule::OnEnginePreExit()
{
	ReleaseMedia();
}

void FDisplayClusterMediaModule::InitializeMedia()
{
	// Runtime only for now
	if (IDisplayCluster::Get().GetOperationMode() != EDisplayClusterOperationMode::Cluster)
	{
		UE_LOG(LogDisplayClusterMedia, Warning, TEXT("DisplayClusterMedia is available in 'cluster' operation mode only"));
		return;
	}

	// Check if media enabled
	if (CVarMediaEnabled.GetValueOnGameThread() == 0)
	{
		UE_LOG(LogDisplayClusterMedia, Log, TEXT("nDisplay media subsytem is disabled by a cvar"));
		return;
	}

	// Instantiate latency queue
	FrameQueue.Init();

	// Parse DCRA configuration and initialize media
	if (const ADisplayClusterRootActor* const RootActor = IDisplayCluster::Get().GetGameMgr()->GetRootActor())
	{
		const FString ClusterNodeId = IDisplayCluster::Get().GetClusterMgr()->GetNodeId();
		if (const UDisplayClusterConfigurationClusterNode* const ClusterNode = RootActor->GetConfigData()->Cluster->GetNode(ClusterNodeId))
		{
			///////////////////////////////
			// Node backbuffer media setup
			{
				const FDisplayClusterConfigurationMedia& MediaSettings = ClusterNode->Media;

				if (MediaSettings.bEnable)
				{
					// Media input
					if (MediaSettings.MediaSource)
					{
						const FString MediaInputId = RootActor->GetName() + FString("_") + ClusterNodeId + FString("_backbuffer_input");

						UE_LOG(LogDisplayClusterMedia, Log, TEXT("Initializing backbuffer media input for node '%s'..."), *ClusterNodeId);

						InputNode = MakeUnique<FDisplayClusterMediaInputNode>(
							MediaInputId, ClusterNodeId,
							MediaSettings.MediaSource);

						AllInputs.Add(InputNode.Get());
					}

					// Media capture
					if (MediaSettings.MediaOutput)
					{
						const FString MediaCaptureId = RootActor->GetName() + FString("_") + ClusterNodeId + FString("_backbuffer_capture");

						UE_LOG(LogDisplayClusterMedia, Log, TEXT("Initializing backbuffer media capture for node '%s'..."), *ClusterNodeId);

						CaptureNode = MakeUnique<FDisplayClusterMediaCaptureNode>(
							MediaCaptureId, ClusterNodeId,
							MediaSettings.MediaOutput);

						AllCaptures.Add(CaptureNode.Get());
					}
				}
			}

			///////////////////////////////
			// Viewports media setup
			for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportIt : ClusterNode->Viewports)
			{
				if (const UDisplayClusterConfigurationViewport* const Viewport = ViewportIt.Value)
				{
					const FDisplayClusterConfigurationMedia& MediaSettings = Viewport->RenderSettings.Media;

					if (MediaSettings.bEnable)
					{
						// Media input
						if (MediaSettings.MediaSource)
						{
							const FString MediaInputId = RootActor->GetName() + FString("_") + ClusterNodeId + FString("_") + ViewportIt.Key + FString("_viewport_input");

							UE_LOG(LogDisplayClusterMedia, Log, TEXT("Initializing viewport media input for viewport '%s'..."), *ViewportIt.Key);

							TUniquePtr<FDisplayClusterMediaInputViewport> NewViewportInput = MakeUnique<FDisplayClusterMediaInputViewport>(
								MediaInputId, ClusterNodeId,
								ViewportIt.Key,
								MediaSettings.MediaSource);

							AllInputs.Add(NewViewportInput.Get());
							InputViewports.Emplace(MediaInputId, MoveTemp(NewViewportInput));
						}

						// Media capture
						if (MediaSettings.MediaOutput)
						{
							const FString MediaCaptureId = RootActor->GetName() + FString("_") + ClusterNodeId + FString("_") + ViewportIt.Key + FString("_viewport_capture");

							UE_LOG(LogDisplayClusterMedia, Log, TEXT("Initializing viewport capture for viewport '%s'..."), *ViewportIt.Key);

							TUniquePtr<FDisplayClusterMediaCaptureViewport> NewViewportCapture = MakeUnique<FDisplayClusterMediaCaptureViewport>(
								MediaCaptureId, ClusterNodeId,
								ViewportIt.Key,
								MediaSettings.MediaOutput);

							AllCaptures.Add(NewViewportCapture.Get());
							CaptureViewports.Emplace(MediaCaptureId, MoveTemp(NewViewportCapture));
						}
					}
				}
			}

			///////////////////////////////
			// ICVFX media setup
			{
				TArray<UActorComponent*> ICVFXCameraComponents;
				RootActor->GetComponents(UDisplayClusterICVFXCameraComponent::StaticClass(), ICVFXCameraComponents);

				for (const UActorComponent* const Component : ICVFXCameraComponents)
				{
					if (const UDisplayClusterICVFXCameraComponent* const ICVFXCamera = Cast<UDisplayClusterICVFXCameraComponent>(Component))
					{
						const FDisplayClusterConfigurationMediaICVFX& MediaSettings = ICVFXCamera->CameraSettings.RenderSettings.Media;

						if (MediaSettings.bEnable)
						{
							const FString ICVFXCameraName = ICVFXCamera->GetName();
							const FString ICVFXViewportId = DisplayClusterMediaHelpers::GenerateICVFXViewportName(ClusterNodeId, ICVFXCameraName);
							const FString MediaCaptureId  = RootActor->GetName() + FString("_") + ICVFXCameraName + FString("_icvfx_capture");
							const FString MediaInputId    = RootActor->GetName() + FString("_") + ICVFXCameraName + FString("_icvfx_input");

							// Media input only
							if (UMediaSource* MediaSource = MediaSettings.GetMediaSource(ClusterNodeId))
							{
								UE_LOG(LogDisplayClusterMedia, Log, TEXT("Initializing ICVFX media input for camera '%s'..."), *ICVFXCameraName);

								TUniquePtr<FDisplayClusterMediaInputViewport> NewICVFXInput = MakeUnique<FDisplayClusterMediaInputViewport>(
									MediaInputId, ClusterNodeId,
									ICVFXViewportId,
									MediaSource);

								AllInputs.Add(NewICVFXInput.Get());
								InputViewports.Emplace(MediaInputId, MoveTemp(NewICVFXInput));
							}

							// Media capture only
							if (UMediaOutput* MediaOutput = MediaSettings.GetMediaOutput(ClusterNodeId))
							{
								UE_LOG(LogDisplayClusterMedia, Log, TEXT("Initializing ICVFX capture for camera '%s'..."), *ICVFXCameraName);

								TUniquePtr<FDisplayClusterMediaCaptureViewport> NewICVFXCapture = MakeUnique<FDisplayClusterMediaCaptureCamera>(
									MediaCaptureId, ClusterNodeId,
									ICVFXCameraName, ICVFXViewportId,
									MediaOutput);

								AllCaptures.Add(NewICVFXCapture.Get());
								CaptureViewports.Emplace(MediaCaptureId, MoveTemp(NewICVFXCapture));
							}
						}
					}
				}
			}
		}
	}
}

void FDisplayClusterMediaModule::ReleaseMedia()
{
	StopCapture();
	StopMedia();

	CaptureViewports.Reset();
	CaptureNode.Reset();
	AllCaptures.Reset();

	InputViewports.Reset();
	InputNode.Reset();
	AllInputs.Reset();

	FrameQueue.Release();
}

void FDisplayClusterMediaModule::StartCapture()
{
	for (FDisplayClusterMediaCaptureBase* Capture : AllCaptures)
	{
		Capture->StartCapture();
	}
}

void FDisplayClusterMediaModule::StopCapture()
{
	for (FDisplayClusterMediaCaptureBase* Capture : AllCaptures)
	{
		Capture->StopCapture();
	}
}

void FDisplayClusterMediaModule::PlayMedia()
{
	for (FDisplayClusterMediaInputBase* MediaInput : AllInputs)
	{
		MediaInput->Play();
	}
}

void FDisplayClusterMediaModule::StopMedia()
{
	for (FDisplayClusterMediaInputBase* MediaInput : AllInputs)
	{
		MediaInput->Stop();
	}
}


IMPLEMENT_MODULE(FDisplayClusterMediaModule, DisplayClusterMedia);
