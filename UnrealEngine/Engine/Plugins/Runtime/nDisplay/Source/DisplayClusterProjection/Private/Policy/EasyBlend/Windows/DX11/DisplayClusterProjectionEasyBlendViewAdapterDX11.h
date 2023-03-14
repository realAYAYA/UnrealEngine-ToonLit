// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/EasyBlend/DisplayClusterProjectionEasyBlendViewAdapterBase.h"

THIRD_PARTY_INCLUDES_START
#include "EasyBlendSDKFrustum.h"
#include "EasyBlendSDKDXStructs.h"
THIRD_PARTY_INCLUDES_END


class FDisplayClusterProjectionEasyBlendViewAdapterDX11
	: public FDisplayClusterProjectionEasyBlendViewAdapterBase
{
public:
	FDisplayClusterProjectionEasyBlendViewAdapterDX11(const FDisplayClusterProjectionEasyBlendViewAdapterBase::FInitParams& InitParams);
	virtual ~FDisplayClusterProjectionEasyBlendViewAdapterDX11();

public:
	virtual bool Initialize(IDisplayClusterViewport* InViewport, const FString& File) override;

public:
	virtual bool CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) override;
	virtual bool GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix) override;
	virtual bool ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy) override;

	static bool IsEasyBlendRenderingEnabled();

protected:
	void ImplInitializeResources_RenderThread();
	bool ImplApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 ContextNum, FRHITexture2D* InputTextures, FRHITexture2D* OutputTextures);

private:
	float ZNear;
	float ZFar;

	struct FViewData
	{
		TUniquePtr<EasyBlendSDKDX_Mesh> EasyBlendMeshData;
		bool bIsMeshInitialized = false;
		
		FViewData()
		{
			EasyBlendMeshData.Reset(new EasyBlendSDKDX_Mesh);
		}

		~FViewData() = default;
	};

	TArray<FViewData> Views;

	bool bIsRenderResourcesInitialized = false;
	FCriticalSection RenderingResourcesInitializationCS;
	FCriticalSection DllAccessCS;
};
