// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyNvidia.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterTypesConverter.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

#include "ID3D11DynamicRHI.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include <nvapi.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"


// TEMPORARY DIAGNOSTICS START
static TAutoConsoleVariable<int32> CVarNvidiaSyncDiagnosticsInit(
	TEXT("nDisplay.sync.nvidia.diag.init"),
	1,
	TEXT("NVAPI diagnostics: init\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n")
	,
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNvidiaSyncDiagnosticsPresent(
	TEXT("nDisplay.sync.nvidia.diag.present"),
	1,
	TEXT("NVAPI diagnostics: present\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n")
	,
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNvidiaSyncDiagnosticsWaitQueue(
	TEXT("nDisplay.sync.nvidia.diag.waitqueue"),
	0,
	TEXT("NVAPI diagnostics: wait queue\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n")
	,
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNvidiaSyncDiagnosticsCompletion(
	TEXT("nDisplay.sync.nvidia.diag.completion"),
	1,
	TEXT("NVAPI diagnostics: frame completion\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n")
	,
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);
// TEMPORARY DIAGNOSTICS END

// CVarNvidiaPresentBarrierCountLimit is used to set the number of presentation cluster sync barriers before getting disabled.
// It is meant to compensate for the fact that the node may not enter the Swap Group barrier on the first frame,
// so an additional barrier may be necessary to ensure sync across the cluster.
// This CVar may become obsolete when the driver can be queried to distinguish between pending and actually
// having joined the swap group, which is not possible at the time, and the only indication of this is by looking at the
// HUD that option 8 in ConfigureDriver.exe provides.
static TAutoConsoleVariable<int32> CVarNvidiaPresentBarrierCountLimit(
	TEXT("nDisplay.sync.nvidia.barriercountlimit"),
	120,
	TEXT("Sets a limit to the number of times a sync barrier is enforced before calling nvidia presentation.\n")
	TEXT("Defaults to 120, which corresponds to 5s@24fps or 2s@60fps\n")
	TEXT("    N <= 0 : No limit\n")
	TEXT("    N >= 1 : The presentation sync barrier will only happen N times\n")
	,
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

// Used to help the cluster start up in sync at the expense of performance, but remove the completion
// flag after the specified number of frames to that it can run without the performance hit.
// Requires nDisplay.sync.nvidia.diag.completion to be 1 in order to have an effect.
static TAutoConsoleVariable<int32> CVarNvidiaCompletionCountLimit(
	TEXT("nDisplay.sync.nvidia.completioncountlimit"),
	120,
	TEXT("Sets a limit to the number of frames it waits for GPU completion.\n")
	TEXT("Defaults to 120, which corresponds to 5s@24fps or 2s@60fps\n")
	TEXT("    N <= 0 : No limit\n")
	TEXT("    N >= 1 : The completion will only happen for N frames\n")
	,
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);


namespace DisplayClusterRenderSyncPolicyNvidia_Data_Windows
{
	IUnknown* D3DDevice = nullptr;
	IDXGISwapChain2* DXGISwapChain = nullptr;
}


FDisplayClusterRenderSyncPolicyNvidia::FDisplayClusterRenderSyncPolicyNvidia(const TMap<FString, FString>& Parameters)
	: FDisplayClusterRenderSyncPolicyBase(Parameters)
{
}

FDisplayClusterRenderSyncPolicyNvidia::~FDisplayClusterRenderSyncPolicyNvidia()
{
	using namespace DisplayClusterRenderSyncPolicyNvidia_Data_Windows;

	if (bNvApiInitialized)
	{
		if (bNvApiBarrierSet)
		{
			// Unbind from swap barrier
			NvAPI_D3D1x_BindSwapBarrier(D3DDevice, RequestedGroup, 0);
			// Leave swap group
			NvAPI_D3D1x_JoinSwapGroup(D3DDevice, DXGISwapChain, 0, false);
		}

		NvAPI_Unload();
	}
}

bool FDisplayClusterRenderSyncPolicyNvidia::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	const NvAPI_Status NvApiResult = NvAPI_Initialize();
	if (NvApiResult != NVAPI_OK)
	{
		UE_LOG(LogDisplayClusterRenderSync, Error, TEXT("NvAPI_Initialize() failed, error 0x%x"), NvApiResult);
	}
	else
	{
		bNvApiInitialized = true;
	}

	return bNvApiInitialized;
}

bool FDisplayClusterRenderSyncPolicyNvidia::SynchronizeClusterRendering(int32& InOutSyncInterval)
{
	using namespace DisplayClusterRenderSyncPolicyNvidia_Data_Windows;

	// Initialize barriers at first call
	if (!bNvApiBarrierSet)
	{
		bNvDiagInit       = (CVarNvidiaSyncDiagnosticsInit.GetValueOnRenderThread() != 0);
		bNvDiagPresent    = (CVarNvidiaSyncDiagnosticsPresent.GetValueOnRenderThread() != 0);
		bNvDiagWaitQueue  = (CVarNvidiaSyncDiagnosticsWaitQueue.GetValueOnRenderThread() != 0);
		bNvDiagCompletion = (CVarNvidiaSyncDiagnosticsCompletion.GetValueOnRenderThread() != 0);

		NvPresentBarrierCountLimit = CVarNvidiaPresentBarrierCountLimit.GetValueOnRenderThread();
		NvCompletionCountLimit     = CVarNvidiaCompletionCountLimit.GetValueOnRenderThread();


		UE_LOG(LogDisplayClusterRenderSync, Log, 
			TEXT("NVAPI DIAG: init=%d present=%d waitqueue=%d completion=%d barriercountlimit=%d completioncountdownlimit=%d"), 
			bNvDiagInit ? 1 : 0, 
			bNvDiagPresent ? 1 : 0, 
			bNvDiagWaitQueue ? 1 : 0, 
			bNvDiagCompletion ? 1 : 0, 
			NvPresentBarrierCountLimit,
			NvCompletionCountLimit
		);

		// Set up NVIDIA swap barrier
		bNvApiBarrierSet = InitializeNvidiaSwapLock();
	}

	// Check if all required objects are available
	if (!D3DDevice || !DXGISwapChain)
	{
		UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("Couldn't get DX resources, no swap synchronization will be performed"));
		// Present frame on a higher level
		return true;
	}

	// NVAPI Diagnostics: frame completion
	// Wait unless frame rendering is finished
	if (bNvDiagCompletion)
	{
		UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, TEXT("NVAPI DIAG: completion start"));
		WaitForFrameCompletion();
		UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, TEXT("NVAPI DIAG: completion end"));

		// Disable completion when it reaches the set limit.
		if ((NvCompletionCountLimit > 0) && (++NvCompletionCount >= NvCompletionCountLimit))
		{
			bNvDiagCompletion = false;

			UE_LOG(LogDisplayClusterRenderSync, VeryVerbose,
				TEXT("NVAPI DIAG: Disabled completion after %d frames"),
				NvCompletionCount
			);
		}
	}

	// NVAPI Diagnostics: present
	// Align all threads on the timescale before calling NvAPI_D3D1x_Present
	// As a side-effect, it should avoid NvAPI_D3D1x_Present stuck on application kill
	if (bNvDiagPresent)
	{
		UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, TEXT("NVAPI DIAG: wait start"));
		SyncBarrierRenderThread();
		UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, TEXT("NVAPI DIAG: wait end"));

		// Disable barrier when it reaches the set limit.
		if ((NvPresentBarrierCountLimit > 0) && (++NvPresentBarrierCount >= NvPresentBarrierCountLimit))
		{
			bNvDiagPresent = false;

			UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, 
				TEXT("NVAPI DIAG: Disabled present sync barrier after %d frames"),
				NvPresentBarrierCount
			);
		}
	}

	if (bNvApiBarrierSet && D3DDevice && DXGISwapChain)
	{
		UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, TEXT("NVAPI: presenting the frame with sync..."));

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay NvAPI_D3D1x_Present);

			// Present frame via NVIDIA API
			const NvAPI_Status NvApiResult = NvAPI_D3D1x_Present(D3DDevice, DXGISwapChain, (UINT)InOutSyncInterval, (UINT)0);
			if (NvApiResult != NVAPI_OK)
			{
				UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("NVAPI: An error occurred during frame presentation, error code 0x%x"), NvApiResult);
				// Present frame on a higher level
				return true;
			}
		}

		UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, TEXT("NVAPI: the frame has been presented successfully"));
	}
	else
	{
		UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("NVAPI: Can't synchronize frame presentation"));
		// Something went wrong, let the upper level present this frame
		return true;
	}

	// NVAPI Diagnostics: latency
	// Since we have frame latency set to 1, the frame queue will always be locked right after present call unless the frame is popped out
	// on the closest V-blank. Knowing that, we simply wait unless the frame is empty and then trigger the game thread to start processing next frame.
	if (bNvDiagWaitQueue && DXGISwapChain)
	{
		// If using maximum frame latency, need to manually block on present
		HANDLE FrameLatencyWaitableObjectHandle = DXGISwapChain->GetFrameLatencyWaitableObject();
		if (FrameLatencyWaitableObjectHandle != NULL)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay Wait for queue empty - 'V-blank');

			// Block the current thread until the swap chain has finished presenting.
			if (::WaitForSingleObject(FrameLatencyWaitableObjectHandle, (DWORD)1000) != WAIT_OBJECT_0)
			{
				UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("An error occurred while waiting for the empty frame queue"));
			}

			::CloseHandle(FrameLatencyWaitableObjectHandle);
		}
	}

	// We presented current frame so no need to present it on higher level
	return false;
}

bool FDisplayClusterRenderSyncPolicyNvidia::InitializeNvidiaSwapLock()
{
	using namespace DisplayClusterRenderSyncPolicyNvidia_Data_Windows;

	// Get D3D1XDevice
	D3DDevice = static_cast<IUnknown*>(GDynamicRHI->RHIGetNativeDevice());
	check(D3DDevice);
	
	// Get IDXGISwapChain
	DXGISwapChain = static_cast<IDXGISwapChain2*>(GEngine->GameViewport->Viewport->GetViewportRHI().GetReference()->GetNativeSwapChain());
	check(DXGISwapChain);

	if (!D3DDevice || !DXGISwapChain)
	{
		UE_LOG(LogDisplayClusterRenderSync, Error, TEXT("Couldn't get DX context data for NVIDIA swap barrier initialization"));
		return false;
	}

	// Set frame latency
	if (DXGISwapChain->SetMaximumFrameLatency(1) != S_OK)
	{
		UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("Couldn't set maximum frame latency"));
	}

	NvU32 MaxGroups = 0;
	NvU32 MaxBarriers = 0;

	// Get amount of available groups and barriers
	NvAPI_Status NvApiResult = NvAPI_D3D1x_QueryMaxSwapGroup(D3DDevice, &MaxGroups, &MaxBarriers);
	if (NvApiResult != NVAPI_OK)
	{
		UE_LOG(LogDisplayClusterRenderSync, Error, TEXT("NVAPI: Couldn't query group/barrier limits, error code 0x%x"), NvApiResult);
		return false;
	}

	// Make sure resources are available
	UE_LOG(LogDisplayClusterRenderSync, Log, TEXT("NVAPI: max_groups=%d max_barriers=%d"), (int)MaxGroups, (int)MaxBarriers);
	if (!(MaxGroups > 0 && MaxBarriers > 0))
	{
		UE_LOG(LogDisplayClusterRenderSync, Error, TEXT("NVAPI: No available groups or barriers"));
		return false;
	}

	// Get requested barrier/group from config
	DisplayClusterHelpers::map::template ExtractValueFromString(GetParameters(), FString("SyncGroup"),   RequestedGroup);
	DisplayClusterHelpers::map::template ExtractValueFromString(GetParameters(), FString("SyncBarrier"), RequestedBarrier);

	// NVAPI Diagnostics: init
	// Here we initialize NVIDIA sync on the same frame interval on the timescale
	if (bNvDiagInit)
	{

		UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, TEXT("NVAPI DIAG: init start"));

		// Align all the threads on the same timeline. It's very likely all the nodes got free on the same side of a potential
		// upcoming V-blank. Normally, the barrier synchronization takes up to 500 microseconds which is about 3% of frame time.
		// So the probability of a situation where some nodes left the barrier before V-blank while others after is really low.
		// However, it's still possible. And cluster restart should likely be successfull.
		SyncBarrierRenderThread();

		// Assuming all the nodes in the right place and have some time before V-blank. Let's wait untill it happend.
		WaitForVBlank();

		// We don't really know either we're at the very beginning of V-blank interval or in the end. Let's spend some time
		// on the barrier to let V-blank interval over.
		SyncBarrierRenderThread();

		// Yes, the task scheduler can break the logic above by allocating CPU resources to this thread like 20ms later which
		// makes us behind of the next V-blank for 60Hz output. And we probably should play around thread priorities to reduce
		// probability of that. But hope we're good here. Usually the systems that use NVIDIA sync approach have a lot of CPU
		// cores and don't run any other applications that compete for CPU.
		// Moreover, we will initialize NVIDIA sync for all cluster nodes before presenting first frame. So the initialization
		// process should be fine anyway.

		UE_LOG(LogDisplayClusterRenderSync, VeryVerbose, TEXT("NVAPI DIAG: init end"));
	}

	// Join swap group
	NvApiResult = NvAPI_D3D1x_JoinSwapGroup(D3DDevice, DXGISwapChain, RequestedGroup, true);
	if (NvApiResult != NVAPI_OK)
	{
		UE_LOG(LogDisplayClusterRenderSync, Error, TEXT("NVAPI: Couldn't join swap group %d, error code 0x%x"), RequestedGroup, NvApiResult);
		return false;
	}
	else
	{
		UE_LOG(LogDisplayClusterRenderSync, Log, TEXT("NVAPI: Successfully joined the swap group %d"), RequestedGroup);
	}

	// Bind to sync barrier
	NvApiResult = NvAPI_D3D1x_BindSwapBarrier(D3DDevice, RequestedGroup, RequestedBarrier);
	if (NvApiResult != NVAPI_OK)
	{
		UE_LOG(LogDisplayClusterRenderSync, Error, TEXT("NVAPI: Couldn't bind group %d to swap barrier %d, error code 0x%x"), RequestedGroup, RequestedBarrier, NvApiResult);
		return false;
	}
	else
	{
		UE_LOG(LogDisplayClusterRenderSync, Log, TEXT("NVAPI: Successfully bound group %d to the swap barrier %d"), RequestedGroup, RequestedBarrier);
	}

	// Set barrier initialization flag
	bNvApiBarrierSet = true;

	return true;
}
