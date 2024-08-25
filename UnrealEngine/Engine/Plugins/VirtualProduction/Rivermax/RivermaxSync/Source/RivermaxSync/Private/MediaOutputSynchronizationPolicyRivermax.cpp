// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaOutputSynchronizationPolicyRivermax.h"

#include "Cluster/IDisplayClusterClusterManager.h"
#include "HAL/IConsoleManager.h"
#include "IDisplayCluster.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "RivermaxMediaCapture.h"
#include "RivermaxPTPUtils.h"
#include "RivermaxSyncLog.h"



namespace UE::RivermaxSync
{
	static TAutoConsoleVariable<float> CVarRivermaxSyncWakeupOffset(
		TEXT("Rivermax.Sync.WakeUpOffset"), 0.5f,
		TEXT("Offset from alignment point to wake up at when barrier stalls the cluster. Units: milliseconds"),
		ECVF_Default);

	static TAutoConsoleVariable<bool> CVarRivermaxSyncEnableSelfRepair(
		TEXT("Rivermax.Sync.EnableSelfRepair"), true,
		TEXT("Whether to use exchanged data in the synchronization barrier to detect desynchronized state and act on it to self repair"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxSyncMaxFrameTimeRange(
		TEXT("Rivermax.Sync.MaxFrameTimeRange"), 8,
		TEXT("Maximum number of frame last presented frame from nodes can be from each other when they join the barrier to enable self repair"),
		ECVF_Default);

	static bool GbTriggerRandomDesync = false;
	FAutoConsoleVariableRef CVarTriggerRandomDesync(
		TEXT("Rivermax.Sync.ForceDesync")
		, UE::RivermaxSync::GbTriggerRandomDesync
		, TEXT("After barrier synchronization, trigger random stall."), ECVF_Cheat);
}

FMediaOutputSynchronizationPolicyRivermaxHandler::FMediaOutputSynchronizationPolicyRivermaxHandler(UMediaOutputSynchronizationPolicyRivermax* InPolicyObject)
	: Super(InPolicyObject)
	, MarginMs(InPolicyObject->MarginMs)
{
	// Allocate memory to store the data exchanged in the barrier
	BarrierData.SetNumZeroed(sizeof(FMediaSyncBarrierData));
}

TSubclassOf<UDisplayClusterMediaOutputSynchronizationPolicy> FMediaOutputSynchronizationPolicyRivermaxHandler::GetPolicyClass() const
{
	return UMediaOutputSynchronizationPolicyRivermax::StaticClass();
}

bool FMediaOutputSynchronizationPolicyRivermaxHandler::IsCaptureTypeSupported(UMediaCapture* MediaCapture) const
{
	// We need to make sure:
	// - it's RivermaxCapture
	// - it uses PTP or System time source
	// - it uses AlignmentPoint alignment mode
	if (URivermaxMediaCapture* RmaxCapture = Cast<URivermaxMediaCapture>(MediaCapture))
	{
		using namespace UE::RivermaxCore;

		if (TSharedPtr<IRivermaxManager> RivermaxMgr = IRivermaxCoreModule::Get().GetRivermaxManager())
		{
			const ERivermaxTimeSource TimeSource = RivermaxMgr->GetTimeSource();

			if (TimeSource == ERivermaxTimeSource::PTP || TimeSource == ERivermaxTimeSource::System)
			{
				const FRivermaxOutputStreamOptions Options = RmaxCapture->GetOutputStreamOptions();

				if (Options.AlignmentMode == ERivermaxAlignmentMode::AlignmentPoint)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FMediaOutputSynchronizationPolicyRivermaxHandler::Synchronize()
{
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RmaxSync::Synchronize);

		// Sync on the barrier if everything is good
		if (!IsRunning())
		{
			UE_LOG(LogRivermaxSync, Warning, TEXT("'%s': Synchronization is off"), *GetMediaDeviceId());
			return;
		}

		IDisplayClusterGenericBarriersClient* const BarrierClient = GetBarrierClient();
		if (!BarrierClient || !BarrierClient->IsConnected())
		{
			UE_LOG(LogRivermaxSync, Warning, TEXT("'%s': Barrier client is not connected or nullptr"), *GetMediaDeviceId());
			return;
		}

		UE_LOG(LogRivermaxSync, Verbose, TEXT("'%s': Synchronizing caller '%s' at the barrier '%s'"), *GetMediaDeviceId(), *GetThreadMarker(), *GetBarrierId());

		URivermaxMediaCapture* RmaxCapture = Cast<URivermaxMediaCapture>(CapturingDevice);
		if (RmaxCapture == nullptr || RmaxCapture->GetState() != EMediaCaptureState::Capturing)
		{
			UE_LOG(LogRivermaxSync, Warning, TEXT("'%s': Rivermax Capture isn't valid or not capturing"), *GetMediaDeviceId());
			return;
		}

		// Verify if we are safe to go inside the barrier.
		{
			// Ask the sync implementation about how much time we have before next synchronization timepoint
			const double TimeLeftSeconds = GetTimeBeforeNextSyncPoint();
			// Convert to seconds
			const double MarginSeconds = double(MarginMs) / 1000;

			// In case we're unsafe, skip the upcoming sync timepoint
			if (TimeLeftSeconds < MarginSeconds)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RmaxSync::MarginProtection);

				const float OffsetTimeSeconds = UE::RivermaxSync::CVarRivermaxSyncWakeupOffset.GetValueOnAnyThread() * 1E-3;
				// Sleep for a bit longer to skip the alignment timepoint
				const float SleepTime = TimeLeftSeconds + OffsetTimeSeconds;

				UE_LOG(LogRivermaxSync, VeryVerbose, TEXT("'%s': TimeLeft(%lf) < Margin(%lf) --> Sleeping for %lf..."),
					*GetMediaDeviceId(), TimeLeftSeconds, MarginSeconds, SleepTime);

				FPlatformProcess::SleepNoStats(SleepTime);
			}
		}


		// We are good to go in the barrier, prepare payload about presented frame
		UE::RivermaxCore::FPresentedFrameInfo FrameInfo;
		RmaxCapture->GetLastPresentedFrameInformation(FrameInfo);

		// Fill the memory to be exchanged by nodes in the barrier.
		FMediaSyncBarrierData BarrierDataStruct(FrameInfo);
		UE_LOG(LogRivermaxSync, VeryVerbose, TEXT("'%s' Entering with %u"), *GetMediaDeviceId(), BarrierDataStruct.LastRenderedFrameNumber);
		FMemory::Memcpy(BarrierData.GetData(), &BarrierDataStruct, sizeof(FMediaSyncBarrierData));

		// We don't use response data for now
		TArray<uint8> ResponseData;

		// Synchronize on a barrier
		BarrierClient->Synchronize(GetBarrierId(), GetThreadMarker(), BarrierData, ResponseData);
	}

	// Debug cvar to potentially stall a node after exiting the barrier and missing alignment points
	if (UE::RivermaxSync::GbTriggerRandomDesync)
	{
		FRandomStream RandomStream(FPlatformTime::Cycles64());
		const bool bTriggerDesync = (RandomStream.FRandRange(0.0, 1.0) > 0.7) ? true : false;
		if (bTriggerDesync)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RmaxSync::ForceBadSync);
			URivermaxMediaCapture* RmaxCapture = Cast<URivermaxMediaCapture>(CapturingDevice);
			double TimeLeftSeconds = GetTimeBeforeNextSyncPoint();
			TimeLeftSeconds += RmaxCapture->GetOutputStreamOptions().FrameRate.AsInterval();
			const float OffsetTimeSeconds = UE::RivermaxSync::CVarRivermaxSyncWakeupOffset.GetValueOnAnyThread() * 1E-3;
			const float SleepTime = TimeLeftSeconds + OffsetTimeSeconds;
			FPlatformProcess::SleepNoStats(SleepTime);
		}

		UE::RivermaxSync::GbTriggerRandomDesync = false;
	}
}

double FMediaOutputSynchronizationPolicyRivermaxHandler::GetTimeBeforeNextSyncPoint() const
{
	if (URivermaxMediaCapture* RmaxCapture = Cast<URivermaxMediaCapture>(CapturingDevice))
	{
		if (RmaxCapture->GetState() == EMediaCaptureState::Capturing)
		{
			using namespace UE::RivermaxCore;

			if (TSharedPtr<IRivermaxManager> RivermaxMgr = IRivermaxCoreModule::Get().GetRivermaxManager())
			{
				// Get current time
				const uint64 CurrentTimeNanosec = RivermaxMgr->GetTime();

				// Get next alignment timepoint
				FRivermaxOutputStreamOptions Options = RmaxCapture->GetOutputStreamOptions();
				const uint64 NextAlignmentTimeNanosec = GetNextAlignmentPoint(CurrentTimeNanosec, Options.FrameRate);

				// Time left
				checkSlow(NextAlignmentTimeNanosec > CurrentTimeNanosec);
				const uint64 TimeLeftNanosec = NextAlignmentTimeNanosec - CurrentTimeNanosec;

				// Return remaining time in seconds
				return double(TimeLeftNanosec * 1E-9);
			}
		}
	}

	// Normally we should never get here. As a fallback approach, return some big time interval
	// to prevent calling thread blocking. 1 second is more than any possible threshold.
	return 1.f;
}

bool FMediaOutputSynchronizationPolicyRivermaxHandler::InitializeBarrier(const FString& SyncInstanceId)
{
	// Base initialization first
	if (!Super::InitializeBarrier(SyncInstanceId))
	{
		UE_LOG(LogRivermaxSync, Warning, TEXT("Couldn't initialize barrier for '%s'"), *GetMediaDeviceId());
		return false;
	}

	// Only setup barrier delegate for primary node to handle barrier data verification
	if (!IDisplayCluster::Get().GetClusterMgr()->IsPrimary())
	{
		return true;
	}

	// Get barrier client
	IDisplayClusterGenericBarriersClient* const BarrierClient = GetBarrierClient();
	if (!BarrierClient || !BarrierClient->IsConnected())
	{
		UE_LOG(LogRivermaxSync, Warning, TEXT("Couldn't access a barrier client for '%s'"), *GetMediaDeviceId());
		return false;
	}

	// Get delegate bound to the specific barrier
	IDisplayClusterGenericBarriersClient::FOnGenericBarrierSynchronizationDelegate* Delegate = BarrierClient->GetBarrierSyncDelegate(GetBarrierId());
	if (!Delegate)
	{
		UE_LOG(LogRivermaxSync, Warning, TEXT("'%s': Couldn't access a barrier delegate for barrier '%s'"), *GetMediaDeviceId(), *GetBarrierId());
		return false;
	}

	// Setup synchronization delegate that will be called on the p-node
	Delegate->BindRaw(this, &FMediaOutputSynchronizationPolicyRivermaxHandler::HandleBarrierSync);

	return true;
}

void FMediaOutputSynchronizationPolicyRivermaxHandler::HandleBarrierSync(FGenericBarrierSynchronizationDelegateData& BarrierSyncData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RmaxSync::BarrierSync);

	if (bHasVerifiedClocks == false)
	{
		bHasVerifiedClocks = true;
		bCanUseSelfRepair = ValidateNodesFrameTime(BarrierSyncData.RequestData);
	}

	if (!bCanUseSelfRepair || UE::RivermaxSync::CVarRivermaxSyncEnableSelfRepair.GetValueOnAnyThread() == false)
	{
		return;
	}

	if (BarrierSyncData.RequestData.Num() <= 0)
	{
		UE_LOG(LogRivermaxSync, Verbose, TEXT("'%s': No data was provided by nodes for sync barrier."), *GetMediaDeviceId())
		return;
	}

	FString FirstFrameNode;
	FMediaSyncBarrierData FirstNodeData;
	bool bFirstNode = true;
	bool bSelfRepairRequired = false;

	for (const TPair<FString, TArray<uint8>>& NodePresentedFrame : BarrierSyncData.RequestData)
	{
		check(NodePresentedFrame.Value.Num() == sizeof(FMediaSyncBarrierData));

		const FMediaSyncBarrierData* const NodeData = reinterpret_cast<const FMediaSyncBarrierData* const>(NodePresentedFrame.Value.GetData());
		
		// We expect all nodes to enter the barrier after presenting the SAME frame at the SAME frame number.
		// If a node enters the barrier a frame late on the other, self repair will be triggered.
		// Samething goes if all nodes didn't present the same frame
		if (bFirstNode)
		{
			bFirstNode = false;
			FirstFrameNode = NodePresentedFrame.Key;
			FirstNodeData = *NodeData;
		}
		else if((FirstNodeData.LastRenderedFrameNumber != NodeData->LastRenderedFrameNumber)
			 || (FirstNodeData.PresentedFrameBoundaryNumber != NodeData->PresentedFrameBoundaryNumber))
		{
			UE_LOG(LogRivermaxSync, Warning, TEXT("Desync detected: Node '%s' presented frame '%u' at frame boundary '%llu', but node '%s' presented frame '%u' at frame boundary '%llu'")
			, *FirstFrameNode
			, FirstNodeData.LastRenderedFrameNumber
			, FirstNodeData.PresentedFrameBoundaryNumber
			, *NodePresentedFrame.Key
			, NodeData->LastRenderedFrameNumber
			, NodeData->PresentedFrameBoundaryNumber);

			bSelfRepairRequired = true;
			break;
		}
	}

	// If repair is required, we stall until we are past the next alignment point to have all scheduler present something and get closer to a synchronized state.
	if (bSelfRepairRequired)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RmaxSync::SelfRepair);

		const double TimeLeftSeconds = GetTimeBeforeNextSyncPoint();
		const float OffsetTimeSeconds = UE::RivermaxSync::CVarRivermaxSyncWakeupOffset.GetValueOnAnyThread() * 1E-3;
		const float SleepTime = TimeLeftSeconds + OffsetTimeSeconds;
		FPlatformProcess::SleepNoStats(SleepTime);
	}
	else
	{
		UE_LOG(LogRivermaxSync, VeryVerbose, TEXT("'%s': Cluster synchronized. Nodes presented frame %u at frame boundary %llu"), *GetMediaDeviceId(), FirstNodeData.LastRenderedFrameNumber, FirstNodeData.PresentedFrameBoundaryNumber);
	}
}

bool FMediaOutputSynchronizationPolicyRivermaxHandler::ValidateNodesFrameTime(const TMap<FString, TArray<uint8>>& NodeRequestData) const
{
	uint64 MinFrameTime = TNumericLimits<uint64>::Max();
	uint64 MaxFrameTime = TNumericLimits<uint64>::Min();
	for (const TPair<FString, TArray<uint8>>& NodePresentedFrame : NodeRequestData)
	{
		check(NodePresentedFrame.Value.Num() == sizeof(FMediaSyncBarrierData));

		const FMediaSyncBarrierData* const NodeData = reinterpret_cast<const FMediaSyncBarrierData* const>(NodePresentedFrame.Value.GetData());
		MinFrameTime = FMath::Min(MinFrameTime, NodeData->PresentedFrameBoundaryNumber);
		MaxFrameTime = FMath::Max(MaxFrameTime, NodeData->PresentedFrameBoundaryNumber);
	}

	const uint64 MaxRange = UE::RivermaxSync::CVarRivermaxSyncMaxFrameTimeRange.GetValueOnAnyThread();
	const uint64 DetectedRange = MaxFrameTime - MinFrameTime;
	if (DetectedRange > MaxRange)
	{	
		UE_LOG(LogRivermaxSync, Warning, TEXT("Self repair can't be enabled. Frame time range (%llu) across cluster too large. Verify PTP clocks to be identical on each node."), DetectedRange);
		return false;
	}
	else
	{
		UE_LOG(LogRivermaxSync, Log, TEXT("Self repair can be used. Frame time range (%llu) across cluster points to a common time reference."), DetectedRange);
		return true;
	}
}

TSharedPtr<IDisplayClusterMediaOutputSynchronizationPolicyHandler> UMediaOutputSynchronizationPolicyRivermax::GetHandler()
{
	if (!Handler)
	{
		Handler = MakeShared<FMediaOutputSynchronizationPolicyRivermaxHandler>(this);
	}

	return Handler;
}

