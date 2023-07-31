// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/DisplayClusterRenderTexture.h"

#include "RenderingThread.h"

//---------------------------------------------------------------------------------------
// FDisplayClusterResourceBulkData
//---------------------------------------------------------------------------------------
class FDisplayClusterResourceBulkData : public FResourceBulkDataInterface
{
public:
	FDisplayClusterResourceBulkData(const void* InData, uint32_t InDataSize)
		: Data(InData)
		, DataSize(InDataSize)
	{ }

public:
	virtual const void* GetResourceBulkData() const
	{
		return Data;
	}

	virtual uint32 GetResourceBulkDataSize() const
	{
		return DataSize;
	}

	virtual void Discard()
	{ }

private:
	const void* Data;
	uint32_t    DataSize;
};

//---------------------------------------------------------------------------------------
// FDisplayClusterRenderTextureResource
//---------------------------------------------------------------------------------------
FDisplayClusterRenderTextureResource::FDisplayClusterRenderTextureResource(EPixelFormat InPixelFormat, uint32_t InWidth, uint32_t InHeight, const void* InTextureData, bool bInHasCPUAccess)
{
	bHasCPUAccess = bInHasCPUAccess;

	const uint32 DataSize = CalculateImageBytes(InWidth, InHeight, 1, InPixelFormat);
	TextureData = FMemory::Malloc(DataSize);
	memcpy(TextureData, InTextureData, DataSize);

	// Create texture from data:
	PixelFormat = InPixelFormat;
	Width = InWidth;
	Height = InHeight;

	if (IsInitialized())
	{
		BeginUpdateResourceRHI(this);
	}

	BeginInitResource(this);
}

FDisplayClusterRenderTextureResource::~FDisplayClusterRenderTextureResource()
{
	ReleaseTextureData();
}

void FDisplayClusterRenderTextureResource::ReleaseTextureData()
{
	if (TextureData != nullptr)
	{
		FMemory::Free(TextureData);
		TextureData = nullptr;

		Width = 0;
		Height = 0;
		PixelFormat = PF_Unknown;
		bHasCPUAccess = false;
	}
}

void FDisplayClusterRenderTextureResource::InitRHI()
{
	check(IsInRenderingThread());

	const uint32 DataSize = CalculateImageBytes(Width, Height, 1, PixelFormat);
	FDisplayClusterResourceBulkData BulkDataInterface(TextureData, DataSize);

	// @todo: Changed for CIS but needs to be fixed.
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterRenderTexture"), Width, Height, PixelFormat)
		.SetFlags(ETextureCreateFlags::ShaderResource)
		.SetBulkData(&BulkDataInterface);

	TextureRHI = RHICreateTexture(Desc);

	// CPU access not required, release from memory
	if (bHasCPUAccess == false)
	{
		ReleaseTextureData();
	}
}

//---------------------------------------------------------------------------------------
// FDisplayClusterRenderTexture
//---------------------------------------------------------------------------------------
FDisplayClusterRenderTexture::FDisplayClusterRenderTexture()
	: PrivateResource(nullptr)
	, PrivateResourceRenderThread(nullptr)
{ }

FDisplayClusterRenderTexture::~FDisplayClusterRenderTexture()
{
	if (PrivateResource)
	{
		FDisplayClusterRenderTextureResource* ToDelete = PrivateResource;
		ENQUEUE_RENDER_COMMAND(DisplayCluster_DeleteResource)([ToDelete](FRHICommandListImmediate& RHICmdList)
		{
			ToDelete->ReleaseResource();
			delete ToDelete;
		});
	}
}

void FDisplayClusterRenderTexture::CreateTexture(EPixelFormat InPixelFormat, uint32_t InWidth, uint32_t InHeight, const void* InTextureData, bool bInHasCPUAccess)
{
	// Release the existing texture resource.
	ReleaseResource();

	//Dedicated servers have no texture internals
	if (FApp::CanEverRender())
	{
		// Create a new texture resource.
		FDisplayClusterRenderTextureResource* NewResource = new FDisplayClusterRenderTextureResource(InPixelFormat, InWidth, InHeight, InTextureData, bInHasCPUAccess);
		SetResource(NewResource);

		if (NewResource)
		{
			BeginInitResource(NewResource);
		}
	}
}

const FDisplayClusterRenderTextureResource* FDisplayClusterRenderTexture::GetResource() const
{
	if (IsInActualRenderingThread() || IsInRHIThread())
	{
		return PrivateResourceRenderThread;
	}
	return PrivateResource;
}

FDisplayClusterRenderTextureResource* FDisplayClusterRenderTexture::GetResource()
{
	if (IsInActualRenderingThread() || IsInRHIThread())
	{
		return PrivateResourceRenderThread;
	}
	return PrivateResource;
}

void FDisplayClusterRenderTexture::SetResource(FDisplayClusterRenderTextureResource* InResource)
{
	check(!IsInActualRenderingThread() && !IsInRHIThread());

	// Each PrivateResource value must be updated in it's own thread because any
	// rendering code trying to access the Resource
	// crash if it suddenly sees nullptr or a new resource that has not had it's InitRHI called.

	PrivateResource = InResource;
	ENQUEUE_RENDER_COMMAND(DisplayCluster_SetResourceRenderThread)([this, InResource](FRHICommandListImmediate& RHICmdList)
	{
		PrivateResourceRenderThread = InResource;
	});
}

void FDisplayClusterRenderTexture::ReleaseResource()
{
	if (PrivateResource)
	{
		FDisplayClusterRenderTextureResource* ToDelete = PrivateResource;
		// Free the resource.
		SetResource(nullptr);
		ENQUEUE_RENDER_COMMAND(DisplayCluster_DeleteResource)([ToDelete](FRHICommandListImmediate& RHICmdList)
		{
			ToDelete->ReleaseResource();
			delete ToDelete;
		});
	}
}
