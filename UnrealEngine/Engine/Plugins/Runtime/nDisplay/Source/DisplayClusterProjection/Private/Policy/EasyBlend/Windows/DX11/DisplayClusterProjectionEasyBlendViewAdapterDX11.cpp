// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/EasyBlend/Windows/DX11/DisplayClusterProjectionEasyBlendViewAdapterDX11.h"
#include "Policy/EasyBlend/Windows/DX11/DisplayClusterProjectionEasyBlendLibraryDX11.h"
#include "Policy/DisplayClusterProjectionPolicyBase.h"

#include "DisplayClusterProjectionLog.h"
#include "Misc/DisplayClusterHelpers.h"

#include "ID3D11DynamicRHI.h"

#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/RendererSettings.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"

//------------------------------------------------------------------------------
// FDisplayClusterProjectionEasyBlendViewAdapterDX11
//------------------------------------------------------------------------------
bool FDisplayClusterProjectionEasyBlendViewAdapterDX11::IsEasyBlendRenderingEnabled()
{
	// Easyblend can render only for game or PIE
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		return true;
	}

	// Now easyblend not support support preview
	return false;
}

FDisplayClusterProjectionEasyBlendViewAdapterDX11::FDisplayClusterProjectionEasyBlendViewAdapterDX11(const FDisplayClusterProjectionEasyBlendViewAdapterBase::FInitParams& InitParams)
	: FDisplayClusterProjectionEasyBlendViewAdapterBase(InitParams)
	, bIsRenderResourcesInitialized(false)
{
	check(InitParams.NumViews > 0)

	Views.AddDefaulted(InitParams.NumViews);
}

FDisplayClusterProjectionEasyBlendViewAdapterDX11::~FDisplayClusterProjectionEasyBlendViewAdapterDX11()
{
	for (FViewData& View : Views)
	{
		if (View.bIsMeshInitialized)
		{
			// Release the mesh data only if it was previously initialized
			DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendUninitializeFunc(View.EasyBlendMeshData.Get());
		}
	}
}

bool FDisplayClusterProjectionEasyBlendViewAdapterDX11::Initialize(IDisplayClusterViewport* InViewport, const FString& File)
{
	if (!IsEasyBlendRenderingEnabled())
	{
		return false;
	}

	// Initialize EasyBlend DLL API
	if (!DisplayClusterProjectionEasyBlendLibraryDX11::Initialize())
	{
		if (!FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("Couldn't link to the EasyBlend DLL"));
		}
		return false;
	}

	if(File.IsEmpty())
	{
		if (!FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("File is empty"));
		}
		return false;
	}

	// Check if EasyBlend geometry file exists
	if (!FPaths::FileExists(File))
	{
		UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("File '%s' not found"), *File);
		return false;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay EasyBlend::Initialize);

		// Initialize EasyBlend data for each view
		const auto FileName = StringCast<ANSICHAR>(*File);
		for (FViewData& It : Views)
		{
			// Initialize the mesh data
			{
				FScopeLock lock(&DllAccessCS);

				check(DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendInitializeFunc);
				const EasyBlendSDKDXError Result = DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendInitializeFunc(FileName.Get(), It.EasyBlendMeshData.Get());
				if (!EasyBlendSDKDX_SUCCEEDED(Result))
				{
					UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("Couldn't initialize EasyBlend internals"));
					return false;
				}
			}

			// EasyBlendMeshData has been initialized
			It.bIsMeshInitialized = true;

			// Only perspective projection is supported so far
			if (It.EasyBlendMeshData->Projection != EasyBlendSDKDX_PROJECTION_Perspective)
			{
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("EasyBlend mesh data has projection value %d. Only perspective projection is allowed at this version."), EasyBlendSDKDX_PROJECTION_Perspective);
				return false;
			}
		}
	}

	return true;
}

void FDisplayClusterProjectionEasyBlendViewAdapterDX11::ImplInitializeResources_RenderThread()
{
	check(IsInRenderingThread());
	check(GDynamicRHI);

	if (!bIsRenderResourcesInitialized)
	{
		bIsRenderResourcesInitialized = true;
		FScopeLock Lock(&RenderingResourcesInitializationCS);

		if (IsEasyBlendRenderingEnabled())
		{
			FViewport* MainViewport = GEngine->GameViewport->Viewport;
			if (IsRHID3D11() && MainViewport)
			{
				FD3D11Device* Device = GetID3D11DynamicRHI()->RHIGetDevice();
				FD3D11DeviceContext* DeviceContext = GetID3D11DynamicRHI()->RHIGetDeviceContext();
				if (Device && DeviceContext)
				{
					IDXGISwapChain* SwapChain = GetID3D11DynamicRHI()->RHIGetSwapChain(MainViewport->GetViewportRHI().GetReference());
					if (SwapChain)
					{
						// Create RT texture for viewport warp
						for (FViewData& It : Views)
						{
							// Initialize EasyBlend internals
							check(DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendInitDeviceObjectsFunc);
							EasyBlendSDKDXError sdkErr = DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendInitDeviceObjectsFunc(It.EasyBlendMeshData.Get(), Device, DeviceContext, SwapChain);
							if (EasyBlendSDKDX_FAILED(sdkErr))
							{
								UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("Couldn't initialize EasyBlend Device/DeviceContext/SwapChain"));
							}
						}
					}
				}
			}
		}
	}
}

// Location/Rotation inside the function is in EasyBlend space with a scale applied
bool FDisplayClusterProjectionEasyBlendViewAdapterDX11::CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP)
{
	check(Views.Num() > (int32)InContextNum);

	ZNear = NCP;
	ZFar = FCP;

	// Convert to EasyBlend coordinate system
	FVector EasyBlendEyeLocation;
	EasyBlendEyeLocation.X = InOutViewLocation.Y;
	EasyBlendEyeLocation.Y = -InOutViewLocation.Z;
	EasyBlendEyeLocation.Z = InOutViewLocation.X;

	// View rotation
	double Yaw = 0.l;
	double Pitch = 0.l;
	double Roll = 0.l;

	{
		FScopeLock lock(&DllAccessCS);

		// Update view location
		check(DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetEyepointFunc);
		DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetEyepointFunc(Views[InContextNum].EasyBlendMeshData.Get(), EasyBlendEyeLocation.X, EasyBlendEyeLocation.Y, EasyBlendEyeLocation.Z);

		// Get actual view rotation
		check(DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSDK_GetHeadingPitchRollFunc);
		DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSDK_GetHeadingPitchRollFunc(Yaw, Pitch, Roll, Views[InContextNum].EasyBlendMeshData.Get());
	}

	// Forward view rotation to a caller
	InOutViewRotation = FRotator(-(float)Pitch, (float)Yaw, (float)Roll);

	return true;
}

bool FDisplayClusterProjectionEasyBlendViewAdapterDX11::GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	check(Views.Num() > (int32)InContextNum);

	// Build Projection matrix:
	const float Left   = Views[InContextNum].EasyBlendMeshData->Frustum.LeftAngle;
	const float Right  = Views[InContextNum].EasyBlendMeshData->Frustum.RightAngle;
	const float Bottom = Views[InContextNum].EasyBlendMeshData->Frustum.BottomAngle;
	const float Top    = Views[InContextNum].EasyBlendMeshData->Frustum.TopAngle;

	InViewport->CalculateProjectionMatrix(InContextNum, Left, Right, Top, Bottom, ZNear, ZFar, true);
	OutPrjMatrix = InViewport->GetContexts()[InContextNum].ProjectionMatrix;

	return true;
}

bool FDisplayClusterProjectionEasyBlendViewAdapterDX11::ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
{
	check(IsInRenderingThread());
	check(InViewportProxy);

	// Now easyblend not support supported preview
	if (!IsEasyBlendRenderingEnabled())
	{
		return false;
	}

	// Get in\out remp resources ref from viewport
	TArray<FRHITexture2D*> InputTextures, OutputTextures;
	if (!InViewportProxy->GetResources_RenderThread(EDisplayClusterViewportResourceType::InputShaderResource, InputTextures))
	{
		return false;
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

	// Easyblend require one time initializetion with our D3D11 resources
	ImplInitializeResources_RenderThread();

	TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay EasyBlend::Render);
	for (int32 ContextNum = 0; ContextNum < InputTextures.Num(); ContextNum++)
	{
		if (!ImplApplyWarpBlend_RenderThread(RHICmdList, ContextNum, InputTextures[ContextNum], OutputTextures[ContextNum]))
		{
			return false;
		}
	}

	// warp result is now inside AdditionalRTT.  Later, from the DC ViewportManagerProxy it will be resolved to FrameRTT 
	return true;
}

bool FDisplayClusterProjectionEasyBlendViewAdapterDX11::ImplApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 ContextNum, FRHITexture2D* InputTexture, FRHITexture2D* OutputTexture)
{
	if (!IsEasyBlendRenderingEnabled())
	{
		return false;
	}

	FViewport* MainViewport = GEngine->GameViewport->Viewport;
	if (!IsRHID3D11() || MainViewport == nullptr)
	{
		return false;
	}

	ID3D11DynamicRHI* D3D11RHI = GetID3D11DynamicRHI();

	// Prepare the textures
	ID3D11RenderTargetView* DstTextureRTV = D3D11RHI->RHIGetRenderTargetView(OutputTexture);

	ID3D11Texture2D* DstTextureD3D11 = (ID3D11Texture2D*)D3D11RHI->RHIGetResource(OutputTexture);
	ID3D11Texture2D* SrcTextureD3D11 = (ID3D11Texture2D*)D3D11RHI->RHIGetResource(InputTexture);

	// Setup In/Out EasyBlend textures
	{
		FScopeLock Lock(&DllAccessCS);

		check(DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetInputTexture2DFunc);
		const EasyBlendSDKDXError EasyBlendSDKDXError1 = DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetInputTexture2DFunc(Views[ContextNum].EasyBlendMeshData.Get(), SrcTextureD3D11);

		check(DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetOutputTexture2DFunc);
		const EasyBlendSDKDXError EasyBlendSDKDXError2 = DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetOutputTexture2DFunc(Views[ContextNum].EasyBlendMeshData.Get(), DstTextureD3D11);

		if (!(EasyBlendSDKDX_SUCCEEDED(EasyBlendSDKDXError1) && EasyBlendSDKDX_SUCCEEDED(EasyBlendSDKDXError2)))
		{
			UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("Coulnd't configure in/out textures"));
			return false;
		}
	}

	D3D11_VIEWPORT RenderViewportData;
	RenderViewportData.MinDepth = 0.0f;
	RenderViewportData.MaxDepth = 1.0f;
	RenderViewportData.Width = static_cast<float>(OutputTexture->GetSizeX());
	RenderViewportData.Height = static_cast<float>(OutputTexture->GetSizeY());
	RenderViewportData.TopLeftX = 0.0f;
	RenderViewportData.TopLeftY = 0.0f;

	FD3D11Device* Device = GetID3D11DynamicRHI()->RHIGetDevice();
	FD3D11DeviceContext* DeviceContext = GetID3D11DynamicRHI()->RHIGetDeviceContext();
	if (Device && DeviceContext)
	{

		IDXGISwapChain* SwapChain = D3D11RHI->RHIGetSwapChain(MainViewport->GetViewportRHI().GetReference());
		if (SwapChain)
		{
			DeviceContext->RSSetViewports(1, &RenderViewportData);
			DeviceContext->OMSetRenderTargets(1, &DstTextureRTV, nullptr);
			DeviceContext->Flush();

			{
				FScopeLock Lock(&DllAccessCS);

				// Perform warp&blend by the EasyBlend
				check(DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendDXRenderFunc);
				EasyBlendSDKDXError EasyBlendSDKDXError = DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendDXRenderFunc(
					Views[ContextNum].EasyBlendMeshData.Get(),
					Device,
					DeviceContext,
					SwapChain,
					false);

				if (!EasyBlendSDKDX_SUCCEEDED(EasyBlendSDKDXError))
				{
					UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("EasyBlend couldn't perform rendering operation"));
					return false;
				}

				return true;
			}
		}
	}

	UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("UE couldn't perform EasyBlend rendering on current render device"));
	return false;
}
