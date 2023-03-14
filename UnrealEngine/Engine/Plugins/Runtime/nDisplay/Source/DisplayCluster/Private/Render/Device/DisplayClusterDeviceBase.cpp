// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/DisplayClusterDeviceBase.h"

#include "IPDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Controller/IDisplayClusterClusterNodeController.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Game/IPDisplayClusterGameManager.h"
#include "Render/IPDisplayClusterRenderManager.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterStrings.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterConfigurationTypes.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"

#include "HAL/IConsoleManager.h"

#include "RHIStaticStates.h"
#include "Slate/SceneViewport.h"

#include "Render/PostProcess/IDisplayClusterPostProcess.h"
#include "Render/Presentation/DisplayClusterPresentationBase.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"
#include "Render/Synchronization/IDisplayClusterRenderSyncPolicy.h"


#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Postprocess.h"

#include <utility>

// Enable/Disable ClearTexture for RTT after resolving to the backbuffer
static TAutoConsoleVariable<int32> CVarClearTextureEnabled(
	TEXT("nDisplay.render.ClearTextureEnabled"),
	1,
	TEXT("Enables RTT cleaning for left / mono eye at end of frame.\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n")
	,
	ECVF_RenderThreadSafe
);

FDisplayClusterDeviceBase::FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode InRenderFrameMode)
	: RenderFrameMode(InRenderFrameMode)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Created DCRenderDevice"));
}

FDisplayClusterDeviceBase::~FDisplayClusterDeviceBase()
{
	//@todo: delete singleton object IDisplayClusterViewportManager
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStereoDevice
//////////////////////////////////////////////////////////////////////////////////////////////

bool FDisplayClusterDeviceBase::Initialize()
{
	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return false;
	}

	return true;
}

void FDisplayClusterDeviceBase::StartScene(UWorld* InWorld)
{
}

void FDisplayClusterDeviceBase::EndScene()
{
}

void FDisplayClusterDeviceBase::PreTick(float DeltaSeconds)
{
	if (!bIsCustomPresentSet)
	{
		// Set up our new present handler
		if (MainViewport)
		{
			// Current sync policy
			TSharedPtr<IDisplayClusterRenderSyncPolicy> SyncPolicy = GDisplayCluster->GetRenderMgr()->GetCurrentSynchronizationPolicy();
			check(SyncPolicy.IsValid());

			// Create present handler
			CustomPresentHandler = CreatePresentationObject(MainViewport, SyncPolicy);
			check(CustomPresentHandler);

			const FViewportRHIRef& MainViewportRHI = MainViewport->GetViewportRHI();

			if (MainViewportRHI)
			{
				MainViewportRHI->SetCustomPresent(CustomPresentHandler);
				bIsCustomPresentSet = true;
				GDisplayCluster->GetCallbacks().OnDisplayClusterCustomPresentSet().Broadcast();
			}
			else
			{
				UE_LOG(LogDisplayClusterRender, Error, TEXT("PreTick: MainViewport->GetViewportRHI() returned null reference"));
			}
		}
	}
}

IDisplayClusterPresentation* FDisplayClusterDeviceBase::GetPresentation() const
{
	return CustomPresentHandler;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IStereoRendering
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterDeviceBase::IsStereoEnabled() const
{
	return true;
}

bool FDisplayClusterDeviceBase::IsStereoEnabledOnNextFrame() const
{
	return true;
}

bool FDisplayClusterDeviceBase::EnableStereo(bool stereo /*= true*/)
{
	return true;
}

void FDisplayClusterDeviceBase::InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas)
{
	if (!bIsCustomPresentSet)
	{
		// Set up our new present handler
		if (MainViewport)
		{
			// Current sync policy
			TSharedPtr<IDisplayClusterRenderSyncPolicy> SyncPolicy = GDisplayCluster->GetRenderMgr()->GetCurrentSynchronizationPolicy();
			check(SyncPolicy.IsValid());

			// Create present handler
			CustomPresentHandler = CreatePresentationObject(MainViewport, SyncPolicy);
			check(CustomPresentHandler);

			MainViewport->GetViewportRHI()->SetCustomPresent(CustomPresentHandler);

			GDisplayCluster->GetCallbacks().OnDisplayClusterCustomPresentSet().Broadcast();
		}

		bIsCustomPresentSet = true;
	}
}

EStereoscopicPass FDisplayClusterDeviceBase::GetViewPassForIndex(bool bStereoRequested, int32 ViewIndex) const
{
	if (bStereoRequested)
	{
		if (IsInRenderingThread())
		{
			if (ViewportManagerProxyPtr)
			{
				uint32 ViewportContextNum = 0;
				IDisplayClusterViewportProxy* ViewportProxy = ViewportManagerProxyPtr->FindViewport_RenderThread(ViewIndex, &ViewportContextNum);
				if (ViewportProxy)
				{
					const FDisplayClusterViewport_Context& Context = ViewportProxy->GetContexts_RenderThread()[ViewportContextNum];
					return Context.StereoscopicPass;
				}
			}
		}
		else
		{
			if (ViewportManagerPtr)
			{
				uint32 ViewportContextNum = 0;
				IDisplayClusterViewport* ViewportPtr = ViewportManagerPtr->FindViewport(ViewIndex, &ViewportContextNum);
				if (ViewportPtr)
				{
					const FDisplayClusterViewport_Context& Context = ViewportPtr->GetContexts()[ViewportContextNum];
					return Context.StereoscopicPass;
				}
			}
		}
	}

	return EStereoscopicPass::eSSP_FULL;
}

void FDisplayClusterDeviceBase::AdjustViewRect(int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	check(IsInGameThread());

	if (ViewportManagerPtr == nullptr || ViewportManagerPtr->IsSceneOpened() == false)
	{
		return;
	}
	// ViewIndex == eSSE_MONOSCOPIC(-1) is a special case called for ISR culling math.
	// Since nDisplay is not ISR compatible, we ignore this request. This won't be neccessary once
	// we stop using nDisplay as a stereoscopic rendering device (IStereoRendering).
	else if (ViewIndex < 0)
	{
		return;
	}

	uint32 ViewportContextNum = 0;
	IDisplayClusterViewport* ViewportPtr = ViewportManagerPtr->FindViewport(ViewIndex, &ViewportContextNum);
	if (ViewportPtr == nullptr)
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Viewport StereoViewIndex='%i' not found"), ViewIndex);
		return;
	}

	const FIntRect& ViewRect = ViewportPtr->GetContexts()[ViewportContextNum].RenderTargetRect;

	X = ViewRect.Min.X;
	Y = ViewRect.Min.Y;

	SizeX = ViewRect.Width();
	SizeY = ViewRect.Height();

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Adjusted view rect: Viewport='%s', ViewIndex=%d, [%d,%d - %d,%d]"), *ViewportPtr->GetId(), ViewportContextNum, ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X, ViewRect.Max.Y);
}

void FDisplayClusterDeviceBase::CalculateStereoViewOffset(const int32 ViewIndex, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	check(IsInGameThread());
	check(WorldToMeters > 0.f);

	if (ViewportManagerPtr == nullptr || ViewportManagerPtr->IsSceneOpened() == false)
	{
		return;
	}
	// ViewIndex == eSSE_MONOSCOPIC(-1) is a special case called for ISR culling math.
	// Since nDisplay is not ISR compatible, we ignore this request. This won't be neccessary once
	// we stop using nDisplay as a stereoscopic rendering device (IStereoRendering).
	else if (ViewIndex < 0)
	{
		return;
	}

	uint32 ViewportContextNum = 0;
	IDisplayClusterViewport* ViewportPtr = ViewportManagerPtr->FindViewport(ViewIndex, &ViewportContextNum);
	if (ViewportPtr == nullptr)
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Viewport StereoViewIndex='%i' not found"), ViewIndex);
		return;
	}

	if (!ViewportPtr->GetProjectionPolicy().IsValid())
	{
		// ignore viewports with uninitialized prj policy
		return;
	}

	// Get root actor from viewport
	ADisplayClusterRootActor* const RootActor = ViewportPtr->GetOwner().GetRootActor();
	if (!RootActor)
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("No root actor found in game manager"));
		return;
	}

	const TArray<FDisplayClusterViewport_Context>& ViewportContexts = ViewportPtr->GetContexts();
	const FDisplayClusterViewport_Context& ViewportContext = ViewportContexts[ViewportContextNum];

	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("OLD ViewLoc: %s, ViewRot: %s"), *ViewLocation.ToString(), *ViewRotation.ToString());
	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WorldToMeters: %f"), WorldToMeters);


	// Get camera ID assigned to the viewport
	const FString& CameraId = ViewportPtr->GetRenderSettings().CameraId;

	// Get camera component assigned to the viewport (or default camera if nothing assigned)
	UDisplayClusterCameraComponent* const ViewCamera = (CameraId.IsEmpty() ?
		RootActor->GetDefaultCamera() :
		RootActor->GetComponentByName<UDisplayClusterCameraComponent>(CameraId));

	if (!ViewCamera)
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("No camera found for viewport '%s'"), *ViewportPtr->GetId());
		return;
	}

	if (CameraId.Len() > 0)
	{
		UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Viewport '%s' has assigned camera '%s'"), *ViewportPtr->GetId(), *CameraId);
	}

	// Get the actual camera settings
	const float CfgEyeDist = ViewCamera->GetInterpupillaryDistance();
	const bool  CfgEyeSwap = ViewCamera->GetSwapEyes();
	const float CfgNCP     = GNearClippingPlane;
	const EDisplayClusterEyeStereoOffset CfgEyeOffset = ViewCamera->GetStereoOffset();

	// Calculate eye offset considering the world scale
	const float EyeOffset = CfgEyeDist / 2.f;
	const float EyeOffsetValues[] = { -EyeOffset, 0.f, EyeOffset };

	// Decode current eye type
	EDisplayClusterEyeType EyeType = EDisplayClusterEyeType::Mono;
	if(ViewportContexts.Num() > 1)
	{
		// Support stereo:
		EyeType = (ViewportContextNum == 0)? EDisplayClusterEyeType::StereoLeft : EDisplayClusterEyeType::StereoRight;
	}

	const int32 EyeIndex = (int32)EyeType;

	float PassOffset = 0.f;
	float PassOffsetSwap = 0.f;

	if (EyeType == EDisplayClusterEyeType::Mono)
	{
		// For monoscopic camera let's check if the "force offset" feature is used
		// * Force left (-1) ==> 0 left eye
		// * Force right (1) ==> 2 right eye
		// * Default (0) ==> 1 mono
		const int32 EyeOffsetIdx = 
			(CfgEyeOffset == EDisplayClusterEyeStereoOffset::None ? 0 :
			(CfgEyeOffset == EDisplayClusterEyeStereoOffset::Left ? -1 : 1));

		PassOffset = EyeOffsetValues[EyeOffsetIdx + 1];
		// Eye swap is not available for monoscopic so just save the value
		PassOffsetSwap = PassOffset;
	}
	else
	{
		// For stereo camera we can only swap eyes if required (no "force offset" allowed)
		PassOffset = EyeOffsetValues[EyeIndex];
		PassOffsetSwap = (CfgEyeSwap ? -PassOffset : PassOffset);
	}

	FVector ViewOffset = FVector::ZeroVector;
	if (ViewCamera)
	{
		// View base location
		ViewLocation = ViewCamera->GetComponentLocation();
		ViewRotation = ViewCamera->GetComponentRotation();
		// Apply computed offset to the view location
		const FQuat EyeQuat = ViewRotation.Quaternion();
		ViewOffset = EyeQuat.RotateVector(FVector(0.0f, PassOffsetSwap, 0.0f));
		ViewLocation += ViewOffset;
	}

	// Perform view calculations on a policy side
	if (ViewportPtr->CalculateView(ViewportContextNum, ViewLocation, ViewRotation, ViewOffset, WorldToMeters, CfgNCP, CfgNCP) == false)
	{
#if WITH_EDITOR
		// Hide spam in logs when configuring VP in editor [UE-114493]
		static const bool bIsEditorOperationMode = IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Editor;
		if (!bIsEditorOperationMode)
#endif
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("Couldn't compute view parameters for Viewport %s, ViewIdx: %d"), *ViewportPtr->GetId(), ViewportContextNum);
		}
	}

	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("ViewLoc: %s, ViewRot: %s"), *ViewLocation.ToString(), *ViewRotation.ToString());
}

FMatrix FDisplayClusterDeviceBase::GetStereoProjectionMatrix(const int32 ViewIndex) const
{
	check(IsInGameThread());

	FMatrix PrjMatrix = FMatrix::Identity;

	// ViewIndex == eSSE_MONOSCOPIC(-1) is a special case called for ISR culling math.
	// Since nDisplay is not ISR compatible, we ignore this request. This won't be neccessary once
	// we stop using nDisplay as a stereoscopic rendering device (IStereoRendering).
	if (ViewportManagerPtr && ViewportManagerPtr->IsSceneOpened() && ViewIndex >= 0)
	{
		uint32 ViewportContextNum = 0;
		IDisplayClusterViewport* ViewportPtr = ViewportManagerPtr->FindViewport(ViewIndex, &ViewportContextNum);
		if (ViewportPtr == nullptr)
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("Viewport StereoViewIndex='%i' not found"), ViewIndex);
		}
		else
		if (ViewportPtr->GetProjectionMatrix(ViewportContextNum, PrjMatrix) == false)
		{
			UE_LOG(LogDisplayClusterRender, Warning, TEXT("Got invalid projection matrix: Viewport %s, ViewIdx: %d"), *ViewportPtr->GetId(), ViewportContextNum);
		}
	}
	
	return PrjMatrix;
}

bool FDisplayClusterDeviceBase::BeginNewFrame(FViewport* InViewport, UWorld* InWorld, FDisplayClusterRenderFrame& OutRenderFrame)
{
	check(IsInGameThread());
	check(InViewport);

	IDisplayClusterViewportManagerProxy* NewViewportManagerProxy = nullptr;

	IDisplayCluster& DisplayCluster = IDisplayCluster::Get();
	ADisplayClusterRootActor* RootActor = DisplayCluster.GetGameMgr()->GetRootActor();
	if (RootActor)
	{
		IDisplayClusterViewportManager* ViewportManager = RootActor->GetViewportManager();
		if (ViewportManager)
		{
			const FString LocalNodeId = DisplayCluster.GetConfigMgr()->GetLocalNodeId();
			// Update local node viewports (update\create\delete) and build new render frame
			if (ViewportManager->UpdateConfiguration(RenderFrameMode, LocalNodeId, RootActor))
			{
				if (ViewportManager->BeginNewFrame(InViewport, InWorld, OutRenderFrame))
				{
					// Begin use viewport manager for current frame
					ViewportManagerPtr = ViewportManager;

					// Send viewport manager proxy on render thread
					NewViewportManagerProxy = ViewportManager->GetProxy();

					// update total number of views for this frame (in multiple families)
					DesiredNumberOfViews = OutRenderFrame.DesiredNumberOfViews;
				}
			}
		}
	}

	// Update render thread viewport manager proxy
	ENQUEUE_RENDER_COMMAND(DisplayClusterDevice_SetViewportManagerPtr)(
		[DCRenderDevice = this, ViewportManagerProxy = NewViewportManagerProxy](FRHICommandListImmediate& RHICmdList)
	{
		DCRenderDevice->ViewportManagerProxyPtr = ViewportManagerProxy;
	});

	return NewViewportManagerProxy != nullptr;
}

void FDisplayClusterDeviceBase::FinalizeNewFrame()
{
	IDisplayCluster& DisplayCluster = IDisplayCluster::Get();
	ADisplayClusterRootActor* RootActor = DisplayCluster.GetGameMgr()->GetRootActor();
	if (RootActor)
	{
		IDisplayClusterViewportManager* ViewportManager = RootActor->GetViewportManager();
		if (ViewportManager)
		{
			ViewportManager->FinalizeNewFrame();
		}
	}

	// reset viewport manager ptr on game thread
	ViewportManagerPtr = nullptr;
}

DECLARE_GPU_STAT_NAMED(nDisplay_Device_RenderTexture, TEXT("nDisplay RenderDevice::RenderTexture"));

#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"

void FDisplayClusterDeviceBase::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* BackBuffer, FRHITexture* SrcTexture, FVector2D WindowSize) const
{
	SCOPED_GPU_STAT(RHICmdList, nDisplay_Device_RenderTexture);
	SCOPED_DRAW_EVENT(RHICmdList, nDisplay_Device_RenderTexture);

	if (SrcTexture && BackBuffer)
	{
		// SrcTexture contain MONO/LEFT eye with debug canvas
		// copy the render target texture to the MONO/LEFT_EYE back buffer  (MONO = mono, side_by_side, top_bottom)
		{
			const FIntPoint SrcSize = SrcTexture->GetSizeXY();
			const FIntPoint DstSize = BackBuffer->GetSizeXY();

			FRHICopyTextureInfo CopyInfo;
			CopyInfo.Size.X = FMath::Min(SrcSize.X, DstSize.X);
			CopyInfo.Size.Y = FMath::Min(SrcSize.Y, DstSize.Y);

			TransitionAndCopyTexture(RHICmdList, SrcTexture, BackBuffer, CopyInfo);
		}

		if (RenderFrameMode == EDisplayClusterRenderFrameMode::Stereo && ViewportManagerProxyPtr)
		{
			// QuadBufStereo: Copy RIGHT_EYE to backbuffer
			ViewportManagerProxyPtr->ResolveFrameTargetToBackBuffer_RenderThread(RHICmdList, 1, 1, BackBuffer, WindowSize);
		}

		const bool bClearTextureEnabled = CVarClearTextureEnabled.GetValueOnRenderThread() != 0;
		if (bClearTextureEnabled)
		{
			// Clear render target before out frame resolving, help to make things look better visually for console/resize, etc.
			RHICmdList.Transition(FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
			ClearRenderTarget(RHICmdList, SrcTexture);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IStereoRenderTargetManager
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterDeviceBase::UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget)
{
	// Store viewport
	if (!MainViewport)
	{
		// UE viewport
		MainViewport = (FViewport*)&Viewport;
	}
}

void FDisplayClusterDeviceBase::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	InOutSizeX = FMath::Max(1, (int32)InOutSizeX);
	InOutSizeY = FMath::Max(1, (int32)InOutSizeY);
}

bool FDisplayClusterDeviceBase::NeedReAllocateViewportRenderTarget(const class FViewport& Viewport)
{
	check(IsInGameThread());

	// Get current RT size
	const FIntPoint rtSize = Viewport.GetRenderTargetTextureSizeXY();

	// Get desired RT size
	uint32 newSizeX = rtSize.X;
	uint32 newSizeY = rtSize.Y;

	CalculateRenderTargetSize(Viewport, newSizeX, newSizeY);

	// Here we conclude if need to re-allocate
	const bool Result = (newSizeX != rtSize.X || newSizeY != rtSize.Y);

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Is reallocate viewport render target needed: %d"), Result ? 1 : 0);

	if (Result)
	{
		UE_LOG(LogDisplayClusterRender, Log, TEXT("Need to re-allocate render target: cur %d:%d, new %d:%d"), rtSize.X, rtSize.Y, newSizeX, newSizeY);
	}

	return Result;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterDeviceBase
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterDeviceBase::StartFinalPostprocessSettings(struct FPostProcessSettings* StartPostProcessingSettings, const enum EStereoscopicPass StereoPassType, const int32 StereoViewIndex)
{
	check(IsInGameThread());

	// eSSP_FULL pass reserved for UE internal render
	if (StereoPassType != EStereoscopicPass::eSSP_FULL && ViewportManagerPtr)
	{
		IDisplayClusterViewport* ViewportPtr = ViewportManagerPtr->FindViewport(StereoViewIndex);
		if (ViewportPtr)
		{
			ViewportPtr->GetViewport_CustomPostProcessSettings().DoPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Start, StartPostProcessingSettings);
		}
	}
}

bool FDisplayClusterDeviceBase::OverrideFinalPostprocessSettings(struct FPostProcessSettings* OverridePostProcessingSettings, const enum EStereoscopicPass StereoPassType, const int32 StereoViewIndex, float& BlendWeight)
{
	check(IsInGameThread());

	// eSSP_FULL pass reserved for UE internal render
	if (StereoPassType != EStereoscopicPass::eSSP_FULL && ViewportManagerPtr)
	{
		IDisplayClusterViewport* ViewportPtr = ViewportManagerPtr->FindViewport(StereoViewIndex);
		if (ViewportPtr)
		{
			return ViewportPtr->GetViewport_CustomPostProcessSettings().DoPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Override, OverridePostProcessingSettings, &BlendWeight);
		}
	}

	return false;
}

void FDisplayClusterDeviceBase::EndFinalPostprocessSettings(struct FPostProcessSettings* FinalPostProcessingSettings, const enum EStereoscopicPass StereoPassType, const int32 StereoViewIndex)
{
	check(IsInGameThread());

	// eSSP_FULL pass reserved for UE internal render
	if (StereoPassType != EStereoscopicPass::eSSP_FULL && ViewportManagerPtr && FinalPostProcessingSettings != nullptr)
	{
		IDisplayClusterViewport* ViewportPtr = ViewportManagerPtr->FindViewport(StereoViewIndex);
		if (ViewportPtr)
		{
			ViewportPtr->GetViewport_CustomPostProcessSettings().DoPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Final, FinalPostProcessingSettings);

			FPostProcessSettings RequestedFinalPerViewportPPS;
			// Get the final overall cluster + per-viewport PPS from nDisplay
			if (ViewportPtr->GetViewport_CustomPostProcessSettings().DoPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport, &RequestedFinalPerViewportPPS))
			{
				FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings InPPSnDisplay;
				FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyPPSStructConditional(&InPPSnDisplay, &RequestedFinalPerViewportPPS);

				// Get the passed-in cumulative PPS from the game/viewport (includes all PPVs affecting this viewport)
				FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings InPPSCumulative;
				FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyPPSStruct(&InPPSCumulative, FinalPostProcessingSettings);

				// Blend both together with our custom math instead of the default PPS blending
				FDisplayClusterViewportConfigurationHelpers_Postprocess::BlendPostProcessSettings(*FinalPostProcessingSettings, InPPSCumulative, InPPSnDisplay);
			}
		}
	}
}
