// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PixelFormat.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "UnrealClient.h"
#include "Containers/StaticBitArray.h"

///////////////////////////////////////////////////////////////////
struct FDisplayClusterViewportResourceSettings
{
	FDisplayClusterViewportResourceSettings()
		: Size(ForceInit)
		, Format(EPixelFormat(0))
		, bShouldUseSRGB(false)
		, DisplayGamma(1)
		, bIsRenderTargetable(false)
		, NumMips(1)
	{ }

	FDisplayClusterViewportResourceSettings(const FString& InClusterNodeId, const EPixelFormat InFormat, const bool InbShouldUseSRGB, const float InDisplayGamma)
		: Size(ForceInit)
		, Format(InFormat)
		, bShouldUseSRGB(InbShouldUseSRGB)
		, DisplayGamma(InDisplayGamma)
		, bIsRenderTargetable(false)
		, NumMips(1)
		, ClusterNodeId(InClusterNodeId)
	{ }

	FDisplayClusterViewportResourceSettings(const FDisplayClusterViewportResourceSettings& InBaseSettings, const FIntPoint& InSize, const EPixelFormat InFormat, const bool InbIsRenderTargetable = false, const int32 InNumMips = 1)
		: Size(InSize)
		, Format(InFormat == PF_Unknown ? InBaseSettings.Format : InFormat)
		, bShouldUseSRGB(InBaseSettings.bShouldUseSRGB)
		, DisplayGamma(InBaseSettings.DisplayGamma)
		, bIsRenderTargetable(InbIsRenderTargetable)
		, NumMips(InNumMips)
		, ClusterNodeId(InBaseSettings.ClusterNodeId)
	{ }

	FORCEINLINE bool IsResourceSettingsEqual(const FDisplayClusterViewportResourceSettings& In) const
	{
		return (In.Size == Size)
			&& (In.Format == Format)
			&& (In.bShouldUseSRGB == bShouldUseSRGB)
			&& (In.DisplayGamma == DisplayGamma)
			&& (In.bIsRenderTargetable == bIsRenderTargetable)
			&& (In.NumMips == NumMips);
	}

	FORCEINLINE bool IsClusterNodeNameEqual(const FDisplayClusterViewportResourceSettings& In) const
	{
		return ClusterNodeId == In.ClusterNodeId;
	}

	const FIntPoint    Size;
	const EPixelFormat Format;
	const bool         bShouldUseSRGB;

	// Render target params
	const float DisplayGamma;

	// Texture target params
	const bool bIsRenderTargetable;
	const int32 NumMips;

private:
	// Cluster per-node render
	const FString ClusterNodeId;
};

enum class EDisplayClusterViewportResourceState : uint8
{
	Initialized = 0,
	Unused,
	Deleted,

	// Disable reallocation of this resource for the current frame
	DisableReallocate,

	COUNT
};
///////////////////////////////////////////////////////////////////
class FDisplayClusterViewportResource
	: public FTexture
{
public:
	FDisplayClusterViewportResource(const FDisplayClusterViewportResourceSettings& InResourceSettings);
	virtual ~FDisplayClusterViewportResource()
	{ }

public:
	FORCEINLINE const FDisplayClusterViewportResourceSettings& GetResourceSettingsConstRef() const
	{
		return ViewportResourceSettings;
	}

	void RaiseViewportResourceState(const EDisplayClusterViewportResourceState InState)
	{
		ResourceState[(uint8)InState] = true;
	}

	void ClearViewportResourceState(const EDisplayClusterViewportResourceState InState)
	{
		ResourceState[(uint8)InState] = false;
	}

	bool GetViewportResourceState(const EDisplayClusterViewportResourceState InState) const
	{
		return ResourceState[(uint8)InState];
	}

	virtual const FTexture2DRHIRef& GetViewportResourceRHI() const
	{
		return (const FTexture2DRHIRef&)TextureRHI;
	}

	virtual const FTexture2DRHIRef& GetViewportRenderTargetResourceRHI() const
	{
		return (const FTexture2DRHIRef&)TextureRHI;
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override
	{
		return ViewportResourceSettings.Size.X;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const override
	{
		return ViewportResourceSettings.Size.Y;
	}

protected:
	void ImplInitDynamicRHI_RenderTargetResource2D(FTexture2DRHIRef& OutRenderTargetTextureRHI, FTexture2DRHIRef& OutTextureRHI);
	void ImplInitDynamicRHI_TextureResource2D(FTexture2DRHIRef& OutTextureRHI);

private:
	const FDisplayClusterViewportResourceSettings ViewportResourceSettings;

	// resource state for game thread
	TStaticBitArray<(uint8)EDisplayClusterViewportResourceState::COUNT> ResourceState;
};

///////////////////////////////////////////////////////////////////
class FDisplayClusterViewportTextureResource
	: public FDisplayClusterViewportResource
{
public:
	FDisplayClusterViewportTextureResource(const FDisplayClusterViewportResourceSettings& InResourceSettings)
		: FDisplayClusterViewportResource(InResourceSettings)
		, BackbufferFrameOffset(EForceInit::ForceInitToZero)
	{ }

	virtual ~FDisplayClusterViewportTextureResource()
	{ }

public:
	virtual void InitDynamicRHI() override;

	virtual FString GetFriendlyName() const override
	{
		return TEXT("DisplayClusterViewportTextureResource");
	}

public:
	// OutputFrameTargetableResources frame offset on backbuffer (special for 'side_by_side' and 'top_bottom' DCRenderDevices)
	FIntPoint BackbufferFrameOffset;
};

///////////////////////////////////////////////////////////////////
class FDisplayClusterViewportRenderTargetResource
	: public FDisplayClusterViewportResource
	, public FRenderTarget
{
public:
	FDisplayClusterViewportRenderTargetResource(const FDisplayClusterViewportResourceSettings& InResourceSettings)
		: FDisplayClusterViewportResource(InResourceSettings)
	{ }

	virtual ~FDisplayClusterViewportRenderTargetResource()
	{ }

public:
	virtual void InitDynamicRHI() override;

	virtual FIntPoint GetSizeXY() const override
	{
		return GetResourceSettingsConstRef().Size;
	}

	virtual float GetDisplayGamma() const 
	{
		return GetResourceSettingsConstRef().DisplayGamma;
	}

	virtual FString GetFriendlyName() const override 
	{
		return TEXT("DisplayClusterViewportRenderTargetResource");
	}
};
