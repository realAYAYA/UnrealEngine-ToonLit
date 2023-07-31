// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOPolicy.h"

#include "DisplayClusterProjectionStrings.h"
#include "DisplayClusterProjectionLog.h"

#include "IDisplayCluster.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Engine/RendererSettings.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"

#define VIOSO_USE_GRAPHICS_API_D3D11	1
#define VIOSO_USE_GRAPHICS_API_D3D12	1


//-------------------------------------------------------------------------------------------------
// D3D11
//-------------------------------------------------------------------------------------------------
#if VIOSO_USE_GRAPHICS_API_D3D11
#include "ID3D11DynamicRHI.h"
#endif // VIOSO_USE_GRAPHICS_API_D3D11

//-------------------------------------------------------------------------------------------------
// D3D12
//-------------------------------------------------------------------------------------------------
#if VIOSO_USE_GRAPHICS_API_D3D12
#include "ID3D12DynamicRHI.h"
#endif // VIOSO_USE_GRAPHICS_API_D3D12

int32 GDisplayClusterProjectionVIOSOPolicyEnableNearClippingPlane = 0;
static FAutoConsoleVariableRef CVarDisplayClusterProjectionVIOSOPolicyEnableNearClippingPlane(
	TEXT("nDisplay.render.projection.EnableNearClippingPlane.vioso"),
	GDisplayClusterProjectionVIOSOPolicyEnableNearClippingPlane,
	TEXT("Enable NearClippingPlane for VIOSO (0 - disable).\n"),
	ECVF_RenderThreadSafe
);

#if VIOSO_USE_GRAPHICS_API_D3D11
/**
 * Class for caching D3D11 changed values on scope
 */
class FD3D11ContextHelper
{
public:
	/** Initialization constructor: requires the device context. */
	FD3D11ContextHelper()
	{
		FMemory::Memzero(RenderTargetViews, sizeof(RenderTargetViews));
		FMemory::Memzero(Viewports, sizeof(Viewports));
		DepthStencilView = NULL;


		ID3D11DynamicRHI* D3D11RHI = GDynamicRHI ? GetDynamicRHI<ID3D11DynamicRHI>() : nullptr;
		DeviceContext = D3D11RHI ? D3D11RHI->RHIGetDeviceContext() : nullptr;

		if (DeviceContext)
		{
			ViewportsNum = MaxSimultaneousRenderTargets;
			DeviceContext->OMGetRenderTargets(MaxSimultaneousRenderTargets, &RenderTargetViews[0], &DepthStencilView);
			DeviceContext->RSGetViewports(&ViewportsNum, &Viewports[0]);
		}
	}

	/** Destructor. */
	~FD3D11ContextHelper()
	{
		if (DeviceContext)
		{
			// Flush tail commands
			DeviceContext->Flush();

			// Restore
			DeviceContext->OMSetRenderTargets(MaxSimultaneousRenderTargets, &RenderTargetViews[0], DepthStencilView);
			DeviceContext->RSSetViewports(ViewportsNum, &Viewports[0]);
		}

		// OMGetRenderTargets calls AddRef on each RTV/DSV it returns. We need
		// to make a corresponding call to Release.
		for (int32 TargetIndex = 0; TargetIndex < MaxSimultaneousRenderTargets; ++TargetIndex)
		{
			if (RenderTargetViews[TargetIndex] != nullptr)
			{
				RenderTargetViews[TargetIndex]->Release();
			}
		}

		if (DepthStencilView)
		{
			DepthStencilView->Release();
		}
	}

	inline bool AssignD3D11RenderTarget(FRHITexture2D* RenderTargetTexture)
	{
		if (DeviceContext)
		{
			// Set RTV
			ID3D11RenderTargetView* DestTextureRTV = GetID3D11DynamicRHI()->RHIGetRenderTargetView(RenderTargetTexture, 0, -1);
			DeviceContext->OMSetRenderTargets(1, &DestTextureRTV, nullptr);

			// Set viewport
			D3D11_VIEWPORT RenderViewportData;
			RenderViewportData.MinDepth = 0.0f;
			RenderViewportData.MaxDepth = 1.0f;
			RenderViewportData.TopLeftX = 0.0f;
			RenderViewportData.TopLeftY = 0.0f;
			RenderViewportData.Width = RenderTargetTexture->GetSizeX();
			RenderViewportData.Height = RenderTargetTexture->GetSizeY();
			DeviceContext->RSSetViewports(1, &RenderViewportData);

			// Clear RTV
			static FVector4f ClearColor(0, 0, 0, 1);
			DeviceContext->ClearRenderTargetView(DestTextureRTV, &ClearColor[0]);

			DeviceContext->Flush();

			return true;
		}

		return false;
	};

private:
	ID3D11DeviceContext*    DeviceContext;
	ID3D11RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];
	ID3D11DepthStencilView* DepthStencilView;
	D3D11_VIEWPORT          Viewports[MaxSimultaneousRenderTargets];
	uint32                  ViewportsNum;
};
#endif // VIOSO_USE_GRAPHICS_API_D3D11


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterProjectionVIOSOPolicy
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterProjectionVIOSOPolicy::FDisplayClusterProjectionVIOSOPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
	: FDisplayClusterProjectionPolicyBase(ProjectionPolicyId, InConfigurationProjectionPolicy)
{
}

FDisplayClusterProjectionVIOSOPolicy::~FDisplayClusterProjectionVIOSOPolicy()
{
	ImplRelease();
}


const FString& FDisplayClusterProjectionVIOSOPolicy::GetType() const
{
	static const FString Type(DisplayClusterProjectionStrings::projection::VIOSO);
	return Type;
}

bool FDisplayClusterProjectionVIOSOPolicy::HandleStartScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());	

	// Read VIOSO config data from nDisplay config file
	if (!ViosoConfigData.Initialize(GetParameters(), InViewport))
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("Couldn't read VIOSO configuration from the config file for viewport -'%s'"), *InViewport->GetId());
		}

		return false;
	}

	// Find origin component if it exists
	InitializeOriginComponent(InViewport, ViosoConfigData.OriginCompId);

	Views.AddDefaulted(2);

	// Initialize data for all views
	FScopeLock lock(&DllAccessCS);
	for (TSharedPtr<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>& ViewIt : Views)
	{
		ViewIt = MakeShared<FDisplayClusterProjectionVIOSOPolicyViewData>();
	}

	UE_LOG(LogDisplayClusterProjectionVIOSO, Verbose, TEXT("VIOSO policy has been initialized: %s"), *ViosoConfigData.ToString());

	return true;
}

void FDisplayClusterProjectionVIOSOPolicy::HandleEndScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	ImplRelease();
}

void FDisplayClusterProjectionVIOSOPolicy::ImplRelease()
{
	ReleaseOriginComponent();

	// Destroy VIOSO for all views
	FScopeLock lock(&DllAccessCS);
	Views.Reset();
}

bool FDisplayClusterProjectionVIOSOPolicy::CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float InNCP, const float InFCP)
{
	check(Views.IsValidIndex(InContextNum));
	TSharedPtr<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>& CurrentView = Views[InContextNum];
	if (!CurrentView.IsValid())
	{
		return false;
	}

	const float NCP = GDisplayClusterProjectionVIOSOPolicyEnableNearClippingPlane ? InNCP : 1;
	const float FCP = GDisplayClusterProjectionVIOSOPolicyEnableNearClippingPlane ? InFCP : 1;

	// Get view location in local space
	const USceneComponent* const OriginComp = GetOriginComp();
	const FTransform& World2LocalTransform = (OriginComp ? OriginComp->GetComponentTransform() : FTransform::Identity);

	// Get our base camera location and view offset in local space (MPCDI space)
	const FVector  LocalOrigin    = World2LocalTransform.InverseTransformPosition(InOutViewLocation - ViewOffset);
	const FVector  LocalEyeOrigin = World2LocalTransform.InverseTransformPosition(InOutViewLocation);
	const FRotator LocalRotator   = World2LocalTransform.InverseTransformRotation(InOutViewRotation.Quaternion()).Rotator();

	// Get view prj data from VIOSO
	FScopeLock lock(&DllAccessCS);
	if (!CurrentView->UpdateVIOSO(InViewport, InContextNum, LocalEyeOrigin, LocalRotator, WorldToMeters, NCP, FCP))
	{
		if (!FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(InViewport))
		{
			// Vioso api used, but failed inside math. The config base matrix or vioso geometry is invalid
			UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("Couldn't Calculate View for VIOSO viewport '%s'"), *InViewport->GetId());
		}

		return false;
	}

	// Transform rotation to world space
	InOutViewRotation = World2LocalTransform.TransformRotation(CurrentView->ViewRotation.Quaternion()).Rotator();
	InOutViewLocation = World2LocalTransform.TransformPosition(CurrentView->ViewLocation);

	return true;
}

bool FDisplayClusterProjectionVIOSOPolicy::GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	check(IsInGameThread());
	check(Views.IsValidIndex(InContextNum));
	TSharedPtr<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>& CurrentView = Views[InContextNum];
	if (!CurrentView.IsValid())
	{
		return false;
	}

	OutPrjMatrix = CurrentView->ProjectionMatrix;
	
	return true;
}

bool FDisplayClusterProjectionVIOSOPolicy::IsWarpBlendSupported()
{
	return true;
}

void FDisplayClusterProjectionVIOSOPolicy::ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
{
	check(IsInRenderingThread());

	if (!ImplApplyWarpBlend_RenderThread(RHICmdList, InViewportProxy))
	{
		// warp failed, just resolve texture to frame
		InViewportProxy->ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::InputShaderResource, InViewportProxy->GetOutputResourceType_RenderThread());
	}
}

bool FDisplayClusterProjectionVIOSOPolicy::ImplApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
{
	check(IsInRenderingThread());

	// Get in\out remp resources ref from viewport
	TArray<FRHITexture2D*> InputTextures, OutputTextures;

	// Use for input first MipsShader texture if enabled in viewport render settings
	//@todo: test if domeprojection support mips textures as warp input
	//if (!InViewportProxy->GetResources_RenderThread(EDisplayClusterViewportResourceType::MipsShaderResource, InputTextures))
	{
		// otherwise inputshader texture
		if (!InViewportProxy->GetResources_RenderThread(EDisplayClusterViewportResourceType::InputShaderResource, InputTextures))
		{
			// no source textures
			return false;
		}
	}

	// Get output resources with rects
	// warp result is now inside AdditionalRTT.  Later, from the DC ViewportManagerProxy it will be resolved to FrameRTT 
	if (!InViewportProxy->GetResources_RenderThread(EDisplayClusterViewportResourceType::AdditionalTargetableResource, OutputTextures))
	{
		return false;
	}

	check(InputTextures.Num() == OutputTextures.Num());
	check(InViewportProxy->GetContexts_RenderThread().Num() == InputTextures.Num());

	// External SDK not use our RHI flow, call flush to finish resolve context image to input resource
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

	TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay VIOSO::Render);
	{
		FScopeLock lock(&DllAccessCS);

		for (int32 ContextNum = 0; ContextNum < InputTextures.Num(); ContextNum++)
		{
			if (Views[ContextNum].IsValid())
			{
				if (!Views[ContextNum]->RenderVIOSO_RenderThread(RHICmdList, InputTextures[ContextNum], OutputTextures[ContextNum], ViosoConfigData))
				{
					return false;
				}
			}
		}
	}

	// warp result is now inside AdditionalRTT.  Later, from the DC ViewportManagerProxy it will be resolved to FrameRTT 
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterProjectionVIOSOPolicyViewData
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterProjectionVIOSOPolicyViewData::FDisplayClusterProjectionVIOSOPolicyViewData()
{
	const ERHIInterfaceType RHIType = RHIGetInterfaceType();

	// Create warper for view
	if (RHIType == ERHIInterfaceType::D3D11)
	{
		RenderDevice = ERenderDevice::D3D11;
	}
	else if (RHIType == ERHIInterfaceType::D3D12)
	{
		RenderDevice = ERenderDevice::D3D12;
	}
	else
	{
		UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("VIOSO warp projection not supported by '%s' rhi"), GDynamicRHI->GetName());
	}
}

FDisplayClusterProjectionVIOSOPolicyViewData::~FDisplayClusterProjectionVIOSOPolicyViewData()
{
	if (IsValid())
	{
		Warper.Release();
	}
}

bool FDisplayClusterProjectionVIOSOPolicyViewData::IsValid()
{
	return bInitialized && Warper.IsValid();
}

bool FDisplayClusterProjectionVIOSOPolicyViewData::UpdateVIOSO(IDisplayClusterViewport* InViewport, const uint32 InContextNum, const FVector& LocalLocation, const FRotator& LocalRotator, const float WorldToMeters, const float NCP, const float FCP)
{
	if (IsValid())
	{
		ViewLocation = LocalLocation;
		ViewRotation = LocalRotator;

		return Warper.CalculateViewProjection(InViewport, InContextNum, ViewLocation, ViewRotation, ProjectionMatrix, WorldToMeters, NCP, FCP);
	}

	return false;
}

bool FDisplayClusterProjectionVIOSOPolicyViewData::RenderVIOSO_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* ShaderResourceTexture, FRHITexture2D* RenderTargetTexture, const FViosoPolicyConfiguration& InConfigData)
{
	check(IsInRenderingThread());

	// Delayed vioso initialize on render thread
	bool bRequireInitialize = !bInitialized && !Warper.IsValid();

	if (bRequireInitialize || IsValid())
	{
		if (bRequireInitialize)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay Vioso::Initialize);
			InitializeVIOSO_RenderThread(RHICmdList, RenderTargetTexture, InConfigData);
		}

		if (IsValid())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay Vioso::Render);

			switch (RenderDevice)
			{
#ifdef VIOSO_USE_GRAPHICS_API_D3D11
			case ERenderDevice::D3D11:
			{
				FD3D11ContextHelper D3D11ContextHelper;
				ID3D11Texture2D* SourceTexture = static_cast<ID3D11Texture2D*>(ShaderResourceTexture->GetTexture2D()->GetNativeResource());
				if (D3D11ContextHelper.AssignD3D11RenderTarget(RenderTargetTexture) && Warper.Render(SourceTexture, VWB_STATEMASK_STANDARD))
				{
					return true;
				}
				break;
			}
#endif //VIOSO_USE_GRAPHICS_API_D3D11

#ifdef VIOSO_USE_GRAPHICS_API_D3D12
			case ERenderDevice::D3D12:
			{
				// Insert an outer query that encloses the whole batch
				RHICmdList.EnqueueLambda([ViosoViewData = SharedThis(this), RenderTargetTexture = RenderTargetTexture, ShaderResourceTexture= ShaderResourceTexture](FRHICommandList& ExecutingCmdList)
					{
						ID3D12DynamicRHI* D3D12RHI = GetID3D12DynamicRHI();
						const D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = D3D12RHI->RHIGetRenderTargetView(RenderTargetTexture);

						VWB_D3D12_RENDERINPUT RenderInput = {};
						RenderInput.textureResource = D3D12RHI->RHIGetResource(ShaderResourceTexture);
						RenderInput.renderTarget = D3D12RHI->RHIGetResource(RenderTargetTexture);
						RenderInput.rtvHandlePtr = RTVHandle.ptr;

						//experimental: add resource barrier
						D3D12RHI->RHITransitionResource(ExecutingCmdList, RenderTargetTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

						if (ViosoViewData->Warper.Render(&RenderInput, VWB_STATEMASK_DEFAULT_D3D12))
						{
							//experimental: add resource barrier
							D3D12RHI->RHITransitionResource(ExecutingCmdList, RenderTargetTexture, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
						}
					});

				return true;

				break;
			}
#endif // VIOSO_USE_GRAPHICS_API_D3D12

			default:
				break;
			}
		}
	}

	return false;
}

bool FDisplayClusterProjectionVIOSOPolicyViewData::InitializeVIOSO_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* RenderTargetTexture, const FViosoPolicyConfiguration& InConfigData)
{
	if (bInitialized)
	{
		return true;
	}

	bInitialized = true;

	switch (RenderDevice)
	{

#ifdef VIOSO_USE_GRAPHICS_API_D3D11
	case ERenderDevice::D3D11:
	{
		auto UED3DDevice = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());

		FD3D11ContextHelper D3D11ContextHelper;
		if (D3D11ContextHelper.AssignD3D11RenderTarget(RenderTargetTexture) && Warper.Initialize(UED3DDevice, InConfigData))
		{
			return true;
		}
		break;
	}
#endif // VIOSO_USE_GRAPHICS_API_D3D11

#if VIOSO_USE_GRAPHICS_API_D3D12
	case ERenderDevice::D3D12:
	{
		ID3D12CommandQueue* D3D12CommandQueue = GetID3D12DynamicRHI()->RHIGetCommandQueue();
		if (Warper.Initialize(D3D12CommandQueue, InConfigData))
		{
			return true;
		}
		break;
	}
#endif // VIOSO_USE_GRAPHICS_API_D3D12

	default:
		UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("Unsupported render device for VIOSO"));
		break;
	}

	UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("Couldn't initialize VIOSO internals"));
	return false;
}
