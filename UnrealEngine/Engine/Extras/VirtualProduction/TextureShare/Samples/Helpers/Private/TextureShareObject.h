// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"

/**
 * TextureShare object helper
 */
class FTextureShareObject
	: public ITextureShareObject
{
public:
	FTextureShareObject(ITextureShareSDKObject* InTextureShareSDKObject, const FTextureShareObjectDesc& InObjectDesc);
	virtual ~FTextureShareObject();

public:
	//~ITextureShareObject
	virtual const wchar_t* GetName() const override
	{
		return *CoreObjectDesc.ShareName;
	}

	virtual const FTextureShareObjectDesc& GetObjectDesc() const
	{
		return ObjectDesc;
	}

	virtual ITextureShareSDKObject& GetSDKObject() const override
	{
		return *TextureShareSDKObject;
	}

	virtual FTextureShareCoreSyncSettings& GetSyncSettings() override
	{
		bUpdateSyncSettings = true;

		return SyncSettings;
	}

	virtual bool IsFrameSyncActive() const override;

public:
	// Sync
	virtual bool BeginFrame() override;
	virtual bool EndFrame() override;

	virtual bool FrameSync(const ETextureShareSyncStep InSyncStep) override;
	virtual bool FrameSync_RenderThread(const ETextureShareSyncStep InSyncStep) override;

	bool ResourceSync_RenderThread(const FTextureShareCoreResourceDesc& InResourceDesc);

public:
	virtual EResourceState SendTexture(const ITextureShareDeviceContext& InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const ITextureShareImage& InSrcTexture) override;
	virtual EResourceState ReceiveTexture(const ITextureShareDeviceContext& InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const ITextureShareImage& InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters) override;
	virtual EResourceState ReceiveResource(const ITextureShareDeviceContext& InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, ITextureShareResource& InDestResource) override;

public:
	// Data Containers
	virtual FTextureShareCoreData& GetData() override
	{
		return Data;
	}

	virtual FTextureShareCoreProxyData& GetProxyData_RenderThread() override
	{
		return ProxyData;
	}

	virtual const TArraySerializable<FTextureShareCoreObjectData>& GetReceivedData() const override
	{
		return ReceivedCoreObjectData;
	}

	virtual const TArraySerializable<FTextureShareCoreObjectProxyData>& GetReceivedProxyData_RenderThread() const override
	{
		return ReceivedCoreObjectProxyData;
	}

public:
	// Data Helpers
	virtual const FTextureShareCoreSceneViewData* GetSceneViewData(const FTextureShareCoreViewDesc& InViewDesc) const override;
	virtual bool GetReceivedProxyDataFrameMarker(FTextureShareCoreObjectFrameMarker& OutObjectFrameMarker) const override;

protected:
	friend ITextureShareObject;

	// D3D11
	static FTextureShareCoreResourceRequest GetResourceRequestD3D11(const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D11* InTexture);
	EResourceState D3D11SendTexture(const FTextureShareDeviceContextD3D11* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D11* InSrcTexture);
	EResourceState D3D11ReceiveTexture(const FTextureShareDeviceContextD3D11* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D11* InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters);
	EResourceState D3D11ReceiveResource(const FTextureShareDeviceContextD3D11* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, FTextureShareResourceD3D11* InDestResource);

	// D3D12
	static FTextureShareCoreResourceRequest GetResourceRequestD3D12(const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D12* InTexture);
	EResourceState D3D12SendTexture(const FTextureShareDeviceContextD3D12* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D12* InSrcTexture);
	EResourceState D3D12ReceiveTexture(const FTextureShareDeviceContextD3D12* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D12* InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters);
	EResourceState D3D12ReceiveResource(const FTextureShareDeviceContextD3D12* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, FTextureShareResourceD3D12* InDestResource);

	// Vulkan
	static FTextureShareCoreResourceRequest GetResourceRequestVulkan(const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageVulkan* InTexture);
	EResourceState VulkanSendTexture(const FTextureShareDeviceContextVulkan* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageVulkan* InSrcTexture);
	EResourceState VulkanReceiveTexture(const FTextureShareDeviceContextVulkan* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageVulkan* InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters);
	EResourceState VulkanReceiveResource(const FTextureShareDeviceContextVulkan* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, FTextureShareResourceVulkan* InDestResource);

private:
	void UpdateSyncSettings();

private:
	const FTextureShareObjectDesc ObjectDesc;
	ITextureShareSDKObject* TextureShareSDKObject;

	FTextureShareCoreObjectDesc CoreObjectDesc;

	ETextureShareSyncStep CurrentFrameProxySyncStep = ETextureShareSyncStep::Undefined;
	ETextureShareSyncStep DefferedProxySyncStep = ETextureShareSyncStep::Undefined;

	FTextureShareCoreData      Data;
	FTextureShareCoreProxyData ProxyData;

	TArraySerializable<FTextureShareCoreObjectData>      ReceivedCoreObjectData;
	TArraySerializable<FTextureShareCoreObjectProxyData> ReceivedCoreObjectProxyData;

	bool bUpdateSyncSettings = false;
	FTextureShareCoreSyncSettings SyncSettings;
};
