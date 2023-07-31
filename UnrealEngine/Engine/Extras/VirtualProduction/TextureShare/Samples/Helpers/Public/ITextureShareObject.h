// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareSDK.h"
#include "ITextureShareSDKObject.h"

#include "Containers/TextureShareContainers.h"

// Enable using debug version of SDK
#define TEXTURESHARESDK_DEBUG 0

/**
 * TextureShare object helper
 */
class ITextureShareObject
{
public:
	virtual ~ITextureShareObject() = default;
	static ITextureShareObject* CreateInstance(const FTextureShareObjectDesc& InObjectDesc);

public:
	virtual const wchar_t* GetName() const = 0;
	virtual const FTextureShareObjectDesc& GetObjectDesc() const = 0;
	virtual ITextureShareSDKObject& GetSDKObject() const = 0;
	virtual FTextureShareCoreSyncSettings& GetSyncSettings() = 0;

	virtual bool IsFrameSyncActive() const = 0;

public:
	//////////////////////////////////////////////////////////
	// Sync
	//////////////////////////////////////////////////////////
	virtual bool BeginFrame() = 0;
	virtual bool EndFrame() = 0;

	virtual bool FrameSync(const ETextureShareSyncStep InSyncStep) = 0;
	virtual bool FrameSync_RenderThread(const ETextureShareSyncStep InSyncStep) = 0;

public:
	static FTextureShareCoreResourceRequest GetResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const ITextureShareImage& InTexture);

	virtual EResourceState SendTexture(const ITextureShareDeviceContext& InDeviceContext,     const FTextureShareCoreResourceDesc& InResourceDesc, const ITextureShareImage& InSrcTexture) = 0;
	virtual EResourceState ReceiveTexture(const ITextureShareDeviceContext& InDeviceContext,  const FTextureShareCoreResourceDesc& InResourceDesc, const ITextureShareImage& InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters) = 0;
	virtual EResourceState ReceiveResource(const ITextureShareDeviceContext& InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, ITextureShareResource& InDestResource) = 0;

public:
	//////////////////////////////////////////////////////////
	// Data Containers
	//////////////////////////////////////////////////////////

	/**
	 * Reference to Data.
	 * Object data for the current frame in the GameThread.
	 */
	virtual FTextureShareCoreData& GetData() = 0;

	/**
	 * Reference to ProxyData.
	 * Object proxy data for the current frame in the RenderThread.
	 */
	virtual FTextureShareCoreProxyData& GetProxyData_RenderThread() = 0;

	/**
	 * Received Data from connected process objects
	 */
	virtual const TArraySerializable<FTextureShareCoreObjectData>& GetReceivedData() const = 0;

	/**
	 * Received ProxyData from connected process objects
	 */
	virtual const TArraySerializable<FTextureShareCoreObjectProxyData>& GetReceivedProxyData_RenderThread() const = 0;

public:
	//////////////////////////////////////////////////////////
	// Data Helpers
	//////////////////////////////////////////////////////////
	
	virtual const FTextureShareCoreSceneViewData* GetSceneViewData(const FTextureShareCoreViewDesc& InViewDesc) const =0;

	// Before each call to BeginFrame(), the local frame marker will be assigned/updated.
	// After calling BeginFrame(), remote UE processes receive it, send it to render_thread, and return it back to the local process.
	// This function get FrameMarker from received proxy data
	virtual bool GetReceivedProxyDataFrameMarker(FTextureShareCoreObjectFrameMarker& OutObjectFrameMarker) const = 0;
};
