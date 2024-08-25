// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Policy/EasyBlend/IDisplayClusterProjectionEasyBlendPolicyViewData.h"
#include "Policy/EasyBlend/Windows/DX12/DisplayClusterProjectionEasyBlendLibraryDX12.h"
#include "Templates/SharedPointer.h"

/**
* The DX12 implementation for EasyBlend
*/
class FDisplayClusterProjectionEasyBlendPolicyViewDataDX12
	: public IDisplayClusterProjectionEasyBlendPolicyViewData
	, public TSharedFromThis<FDisplayClusterProjectionEasyBlendPolicyViewDataDX12, ESPMode::ThreadSafe>
{
public:
	~FDisplayClusterProjectionEasyBlendPolicyViewDataDX12();

public:
	//BEGIN ~IDisplayClusterProjectionEasyBlendPolicyViewData
	virtual bool Initialize(const FDisplayClusterProjectionEasyBlendPolicyConfiguration& InEasyBlendConfiguration) override;
	virtual bool CalculateWarpBlend(FDisplayClusterProjectionEasyBlendPolicyViewInfo& InOutViewInfo) override;
	virtual bool ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const FDisplayClusterProjectionEasyBlendPolicyViewInfo& InViewInfo, FRHITexture2D* InputTexture, FRHITexture2D* OutputTexture, FRHIViewport* InRHIViewport) override;

	virtual bool HasPreviewMesh() override { return true; }
	virtual bool GetPreviewMeshGeometry(const FDisplayClusterProjectionEasyBlendPolicyConfiguration& InEasyBlendConfiguration, FDisplayClusterProjectionEasyBlendGeometryExportData& OutMeshData) override;
	//END ~~IDisplayClusterProjectionEasyBlendPolicyViewData

protected:
	void ImplRelease();

private:
	// The unique EasyBlend data that useed for warpblend
	TUniquePtr<EasyBlendSDK_Mesh> EasyBlendMeshData;
};
