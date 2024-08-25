// Copyright Epic Games, Inc. All Rights Reserved.

#include "Synchronization/DisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBase.h"

#include "DisplayClusterMediaHelpers.h"
#include "DisplayClusterMediaLog.h"

#include "MediaCapture.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterEnums.h"
#include "DisplayClusterRootActor.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "IDisplayCluster.h"
#include "Cluster/IDisplayClusterClusterManager.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Templates/SharedPointer.h"


FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler(UDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBase* InPolicyObject)
	: BarrierTimeoutMs(InPolicyObject->BarrierTimeoutMs)
{

}

bool FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::StartSynchronization(UMediaCapture* MediaCapture, const FString& MediaId)
{
	// Cluster mode only
	if (IDisplayCluster::Get().GetOperationMode() != EDisplayClusterOperationMode::Cluster)
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Media synchronization is available in cluster mode only"), *MediaId);
		return false;
	}

	// Nothing to do if already running
	if (bIsRunning)
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Synchronization is on already"), *MediaId);
		return true;
	}

	if (!MediaCapture)
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Invalid capture device (nullptr)"), *MediaId);
		return false;
	}

	if (!IsCaptureTypeSupported(MediaCapture))
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Synchronization of media capture '%s' is not supported by this sync policy"), *MediaId, *MediaCapture->GetName());
		return false;
	}

	// Store capture device
	CapturingDevice = MediaCapture;
	MediaDeviceId   = MediaId;

	// Initialize dynamic barrier first
	if (!InitializeBarrier(MediaId))
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Couldn't initialize barrier client"), *MediaId);
		return false;
	}

	CapturingDevice->OnOutputSynchronization.BindSP(this, &FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::ProcessMediaSynchronizationCallback);

	// Update state
	bIsRunning = true;

	return true;
}

void FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::StopSynchronization()
{
	if (bIsRunning)
	{
		// Don't reference capture device
		if (CapturingDevice)
		{
			CapturingDevice->OnOutputSynchronization.Unbind();
			CapturingDevice = nullptr;
		}

		// Release barrier client
		ReleaseBarrier();

		// Update state
		bIsRunning = false;
	}
}

bool FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::IsRunning()
{
	return bIsRunning;
}

bool FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::IsCaptureTypeSupported(UMediaCapture* MediaCapture) const
{
	return true;
}

FString FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::GetMediaDeviceId() const
{
	return MediaDeviceId;
}

IDisplayClusterGenericBarriersClient* const FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::GetBarrierClient() const
{
	return EthernetBarrierClient.Get();
}

const FString& FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::GetBarrierId() const
{
	return BarrierId;
}

const FString& FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::GetThreadMarker() const
{
	return ThreadMarker;
}

void FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::SyncThreadOnBarrier()
{
	// Sync on the barrier if everything is good
	if (bIsRunning && EthernetBarrierClient && EthernetBarrierClient->IsConnected())
	{
		UE_LOG(LogDisplayClusterMediaSync, Verbose, TEXT("'%s': Synchronizing caller '%s' at the barrier '%s'"), *GetMediaDeviceId(), *ThreadMarker, *BarrierId);
		EthernetBarrierClient->Synchronize(BarrierId, ThreadMarker);
	}
}

bool FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::InitializeBarrier(const FString& MediaId)
{
	if (MediaId.IsEmpty())
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Wrong MediaId"), *MediaId);
		return false;
	}

	IDisplayClusterClusterManager& ClusterMgr = *IDisplayCluster::Get().GetClusterMgr();

	// Barriers client ID
	const FString ClientId = FString::Printf(TEXT("CLN_GB_%s"), *MediaId);
	UE_LOG(LogDisplayClusterMediaSync, VeryVerbose, TEXT("'%s': Requesting barrier client '%s'"), *MediaId, *ClientId);

	// Instantiate barrier client
	EthernetBarrierClient = IDisplayCluster::Get().GetClusterMgr()->CreateGenericBarriersClient(ClientId);
	if (!EthernetBarrierClient)
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Couldn't instantiate barrier client '%s'"), *MediaId, *ClientId);
		return false;
	}

	UE_LOG(LogDisplayClusterMediaSync, Verbose, TEXT("'%s': Instantiated barrier client '%s'"), *MediaId, *ClientId);

	if (!EthernetBarrierClient->Connect())
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Barrier client '%s' couldn't establish connection"), *MediaId, *ClientId);
		return false;
	}

	BarrierId     = GenerateBarrierName();
	ThreadMarker  = MediaId;

	// Thread markers
	TArray<FString> ThreadMarkers;
	GenerateListOfThreadMarkers(ThreadMarkers);

	// Create sync barrier
	if (!EthernetBarrierClient->CreateBarrier(BarrierId, ThreadMarkers, BarrierTimeoutMs))
	{
		UE_LOG(LogDisplayClusterMediaSync, Warning, TEXT("'%s': Barrier client '%s' couldn't create barrier '%s'"), *MediaId, *ClientId, *BarrierId);
		EthernetBarrierClient.Reset();
		return false;
	}

	return true;
}

void FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::ReleaseBarrier()
{
	if (EthernetBarrierClient)
	{
		if (EthernetBarrierClient->IsConnected() && !BarrierId.IsEmpty())
		{
			EthernetBarrierClient->ReleaseBarrier(BarrierId);
		}

		EthernetBarrierClient.Reset();
	}
}

FString FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::GenerateBarrierName() const
{
	// Currently we don't have any synchronization groups. This means all the sync policy instances of the same
	// class use the same barrier. If we want to introduce sync groups in the future, the barrier ID should
	// take that group ID/number into account, and encode it into the barrier name.
	//
	// For example, we want two sets of capture devices to run with different output framerate. In this case, we would
	// need to split those sets into different sync groups.
	//
	// However! All media captures are locked to UE rendering pipeline. This means all the captures will run
	// with the same framerate. Therefore we don't need any sync groups so far.
	return GetPolicyClass()->GetName();
}

void FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::GenerateListOfThreadMarkers(TArray<FString>& OutMarkers) const
{
	OutMarkers.Empty();

	UE_LOG(LogDisplayClusterMediaSync, Verbose, TEXT("'%s': Generating thread markers for barrier '%s'..."), *GetMediaDeviceId(), *BarrierId);

	// Get active DCRA
	if (const ADisplayClusterRootActor* const RootActor = IDisplayCluster::Get().GetGameMgr()->GetRootActor())
	{
		// Get config data
		if (const UDisplayClusterConfigurationData* const CfgData = RootActor->GetConfigData())
		{
			// Iterate over cluster nodes
			for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& NodeIt : CfgData->Cluster->Nodes)
			{
				/////////////////////
				// Backbuffer capture
				{
					const FDisplayClusterConfigurationMediaNodeBackbuffer& MediaSettings = NodeIt.Value->MediaSettings;

					if (MediaSettings.bEnable)
					{
						uint8 CaptureIdx = 0;
						for (const FDisplayClusterConfigurationMediaOutput& MediaOutputItem : MediaSettings.MediaOutputs)
						{
							if (IsValid(MediaOutputItem.MediaOutput) && IsValid(MediaOutputItem.OutputSyncPolicy))
							{
								// Pick the same sync policy only
								if (MediaOutputItem.OutputSyncPolicy->GetClass() == GetPolicyClass())
								{
									const FString BackbufferCaptureId = DisplayClusterMediaHelpers::MediaId::GenerateMediaId(
										DisplayClusterMediaHelpers::MediaId::EMediaDeviceType::Output,
										DisplayClusterMediaHelpers::MediaId::EMediaOwnerType::Backbuffer,
										NodeIt.Key, RootActor->GetName(), FString(), CaptureIdx);

									OutMarkers.Add(BackbufferCaptureId);
								}
							}

							++CaptureIdx;
						}
					}
				}

				///////////////////
				// Viewport capture
				{
					// Iterate over viewports
					for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportIt : NodeIt.Value->Viewports)
					{
						const FDisplayClusterConfigurationMediaViewport& MediaSettings = ViewportIt.Value->RenderSettings.Media;

						if (MediaSettings.bEnable)
						{
							uint8 CaptureIdx = 0;
							for (const FDisplayClusterConfigurationMediaOutput& MediaOutputItem : MediaSettings.MediaOutputs)
							{
								if (IsValid(MediaOutputItem.MediaOutput) && IsValid(MediaOutputItem.OutputSyncPolicy))
								{
									// Pick the same sync policy only
									if (MediaOutputItem.OutputSyncPolicy->GetClass() == GetPolicyClass())
									{
										const FString ViewportCaptureId = DisplayClusterMediaHelpers::MediaId::GenerateMediaId(
											DisplayClusterMediaHelpers::MediaId::EMediaDeviceType::Output,
											DisplayClusterMediaHelpers::MediaId::EMediaOwnerType::Viewport,
											NodeIt.Key, RootActor->GetName(), ViewportIt.Key, CaptureIdx);

										OutMarkers.Add(ViewportCaptureId);
									}
								}

								++CaptureIdx;
							}
						}
					}
				}
			}
		}

		////////////////
		// ICVFX capture
		{
			// Get all ICVFX camera components
			TArray<UDisplayClusterICVFXCameraComponent*> ICVFXCameraComponents;
			RootActor->GetComponents(ICVFXCameraComponents);

			// Iterate over ICVFX cameras
			for (UDisplayClusterICVFXCameraComponent* const ICVFXCameraComponent : ICVFXCameraComponents)
			{
				const FDisplayClusterConfigurationMediaICVFX& MediaSettings = ICVFXCameraComponent->CameraSettings.RenderSettings.Media;

				if (MediaSettings.bEnable)
				{
					uint8 CaptureIdx = 0;
					for (const FDisplayClusterConfigurationMediaOutputGroup& MediaOutputGroup : MediaSettings.MediaOutputGroups)
					{
						// Pick the same sync policy only
						if (MediaOutputGroup.MediaOutput && MediaOutputGroup.OutputSyncPolicy)
						{
							if (MediaOutputGroup.OutputSyncPolicy->GetClass() == GetPolicyClass())
							{
								for (const FString& NodeId : MediaOutputGroup.ClusterNodes.ItemNames)
								{
									const FString ICVFXCaptureId = DisplayClusterMediaHelpers::MediaId::GenerateMediaId(
										DisplayClusterMediaHelpers::MediaId::EMediaDeviceType::Output,
										DisplayClusterMediaHelpers::MediaId::EMediaOwnerType::ICVFXCamera,
										NodeId, RootActor->GetName(), ICVFXCameraComponent->GetName(), CaptureIdx);

									OutMarkers.Add(ICVFXCaptureId);
								}
							}
						}

						++CaptureIdx;
					}
				}
			}
		}
	}

	UE_LOG(LogDisplayClusterMediaSync, Verbose, TEXT("'%s': Generated %d thread markers for barrier '%s' "), *GetMediaDeviceId(), OutMarkers.Num(), *BarrierId);
	for (int32 Idx = 0; Idx < OutMarkers.Num(); ++Idx)
	{
		UE_LOG(LogDisplayClusterMediaSync, Verbose, TEXT("'%s': Barrier '%s', marker %d: %s"), *GetMediaDeviceId(), *BarrierId, Idx, *OutMarkers[Idx]);
	}
}

void FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler::ProcessMediaSynchronizationCallback()
{
	UE_LOG(LogDisplayClusterMediaSync, VeryVerbose, TEXT("'%s': Synchronizing capture..."), *GetMediaDeviceId());

	// Pass to the policy implementations
	Synchronize();
}
