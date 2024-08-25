// Copyright Epic Games, Inc. All Rights Reserved.

#include "GlobalRenderResources.h"
#include "RenderGraphUtils.h"
#include "Containers/ResourceArray.h"
#include "RenderCore.h"
#include "RenderUtils.h"
#include "Algo/Reverse.h"
#include "Async/Mutex.h"

// The maximum number of transient vertex buffer bytes to allocate before we start panic logging who is doing the allocations
int32 GMaxVertexBytesAllocatedPerFrame = 32 * 1024 * 1024;

FAutoConsoleVariableRef CVarMaxVertexBytesAllocatedPerFrame(
	TEXT("r.MaxVertexBytesAllocatedPerFrame"),
	GMaxVertexBytesAllocatedPerFrame,
	TEXT("The maximum number of transient vertex buffer bytes to allocate before we start panic logging who is doing the allocations"));

int32 GGlobalBufferNumFramesUnusedThreshold = 30;
FAutoConsoleVariableRef CVarGlobalBufferNumFramesUnusedThreshold(
	TEXT("r.NumFramesUnusedBeforeReleasingGlobalResourceBuffers"),
	GGlobalBufferNumFramesUnusedThreshold,
	TEXT("Number of frames after which unused global resource allocations will be discarded. Set 0 to ignore. (default=30)"));

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Bulk data interface for providing a single color used to initialize a volume texture.
struct FColorBulkData : public FResourceBulkDataInterface
{
	FColorBulkData(uint8 Alpha) : Color(0, 0, 0, Alpha) { }
	FColorBulkData(int32 R, int32 G, int32 B, int32 A) : Color(R, G, B, A) {}
	FColorBulkData(FColor InColor) : Color(InColor) { }

	virtual const void* GetResourceBulkData() const override { return &Color; }
	virtual uint32 GetResourceBulkDataSize() const override { return sizeof(Color); }
	virtual void Discard() override { }

	FColor Color;
};

/**
 * A solid-colored 1x1 texture.
 */
template <int32 R, int32 G, int32 B, int32 A>
class FColoredTexture : public FTextureWithSRV
{
public:
	// FResource interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Create the texture RHI.  		
		FColorBulkData BulkData(R, G, B, A);

		// BGRA typed UAV is unsupported per D3D spec, use RGBA here.
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("ColoredTexture"), 1, 1, PF_R8G8B8A8)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetBulkData(&BulkData)
			.SetClassName(TEXT("FColoredTexture"));

		TextureRHI = RHICreateTexture(Desc);

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Wrap, AM_Wrap, AM_Wrap);
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);

		// Create a view of the texture
		ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(TextureRHI, 0u);
	}

	virtual uint32 GetSizeX() const override { return 1; }
	virtual uint32 GetSizeY() const override { return 1; }
};

class FEmptyVertexBuffer : public FVertexBufferWithSRV
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Create the texture RHI.  		
		FRHIResourceCreateInfo CreateInfo(TEXT("EmptyVertexBuffer"));

		VertexBufferRHI = RHICmdList.CreateVertexBuffer(16u, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);

		// Create a view of the buffer
		ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(VertexBufferRHI, 4u, PF_R32_UINT);
		UnorderedAccessViewRHI = RHICmdList.CreateUnorderedAccessView(VertexBufferRHI, PF_R32_UINT);
	}
};

class FEmptyStructuredBuffer : public FVertexBufferWithSRV
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Create the buffer RHI.  		
		FRHIResourceCreateInfo CreateInfo(TEXT("EmptyStructuredBuffer"));

		const uint32 BufferSize = sizeof(uint32) * 4u;
		VertexBufferRHI = RHICmdList.CreateStructuredBuffer(sizeof(uint32), BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);

		// Create a view of the buffer
		ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(VertexBufferRHI);
		UnorderedAccessViewRHI = RHICmdList.CreateUnorderedAccessView(VertexBufferRHI, false, false);
	}
};

class FBlackTextureWithSRV : public FColoredTexture<0, 0, 0, 255>
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FColoredTexture::InitRHI(RHICmdList);
		FRHITextureReference::DefaultTexture = TextureRHI;
	}

	virtual void ReleaseRHI() override
	{
		FRHITextureReference::DefaultTexture.SafeRelease();
		FColoredTexture::ReleaseRHI();
	}
};

FTextureWithSRV* GWhiteTextureWithSRV = new TGlobalResource<FColoredTexture<255, 255, 255, 255>, FRenderResource::EInitPhase::Pre>;
FTextureWithSRV* GBlackTextureWithSRV = new TGlobalResource<FBlackTextureWithSRV, FRenderResource::EInitPhase::Pre>();
FTextureWithSRV* GTransparentBlackTextureWithSRV = new TGlobalResource<FColoredTexture<0, 0, 0, 0>, FRenderResource::EInitPhase::Pre>;
FTexture* GWhiteTexture = GWhiteTextureWithSRV;
FTexture* GBlackTexture = GBlackTextureWithSRV;
FTexture* GTransparentBlackTexture = GTransparentBlackTextureWithSRV;

FVertexBufferWithSRV* GEmptyVertexBufferWithUAV = new TGlobalResource<FEmptyVertexBuffer, FRenderResource::EInitPhase::Pre>;
FVertexBufferWithSRV* GEmptyStructuredBufferWithUAV = new TGlobalResource<FEmptyStructuredBuffer, FRenderResource::EInitPhase::Pre>;

class FWhiteVertexBuffer : public FVertexBufferWithSRV
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Create the texture RHI.  		
		FRHIResourceCreateInfo CreateInfo(TEXT("WhiteVertexBuffer"));

		VertexBufferRHI = RHICmdList.CreateVertexBuffer(sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);

		FVector4f* BufferData = (FVector4f*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector4f), RLM_WriteOnly);
		*BufferData = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
		RHICmdList.UnlockBuffer(VertexBufferRHI);

		// Create a view of the buffer
		ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(FVector4f), PF_A32B32G32R32F);
	}
};

FVertexBufferWithSRV* GWhiteVertexBufferWithSRV = new TGlobalResource<FWhiteVertexBuffer, FRenderResource::EInitPhase::Pre>;

class FWhiteVertexBufferWithRDG : public FBufferWithRDG
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		if (!Buffer.IsValid())
		{
			Buffer = AllocatePooledBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), 1), TEXT("WhiteVertexBufferWithRDG"));

			FVector4f* BufferData = (FVector4f*)RHICmdList.LockBuffer(Buffer->GetRHI(), 0, sizeof(FVector4f), RLM_WriteOnly);
			*BufferData = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
			RHICmdList.UnlockBuffer(Buffer->GetRHI());
		}
	}
};

FBufferWithRDG* GWhiteVertexBufferWithRDG = new TGlobalResource<FWhiteVertexBufferWithRDG, FRenderResource::EInitPhase::Pre>();

/**
 * A class representing a 1x1x1 black volume texture.
 */
template <EPixelFormat PixelFormat, uint8 Alpha>
class FBlackVolumeTexture : public FTexture
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		if (GSupportsTexture3D)
		{
			// Create the texture.
			FColorBulkData BulkData(Alpha);

			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create3D(TEXT("BlackVolumeTexture3D"), 1, 1, 1, PixelFormat)
				.SetFlags(ETextureCreateFlags::ShaderResource)
				.SetBulkData(&BulkData)
				.SetClassName(TEXT("FBlackVolumeTexture"));

			TextureRHI = RHICreateTexture(Desc);
		}
		else
		{
			// Create a texture, even though it's not a volume texture
			FColorBulkData BulkData(Alpha);

			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("BlackVolumeTexture2D"), 1, 1, PixelFormat)
				.SetFlags(ETextureCreateFlags::ShaderResource)
				.SetBulkData(&BulkData);

			TextureRHI = RHICreateTexture(Desc);
		}

		// Create the sampler state.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Wrap, AM_Wrap, AM_Wrap);
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
	}

	virtual uint32 GetSizeX() const override { return 1; }
	virtual uint32 GetSizeY() const override { return 1; }
};

/** Global black volume texture resource. */
FTexture* GBlackVolumeTexture = new TGlobalResource<FBlackVolumeTexture<PF_B8G8R8A8, 0>, FRenderResource::EInitPhase::Pre>();
FTexture* GBlackAlpha1VolumeTexture = new TGlobalResource<FBlackVolumeTexture<PF_B8G8R8A8, 255>, FRenderResource::EInitPhase::Pre>();

/** Global black volume texture resource. */
FTexture* GBlackUintVolumeTexture = new TGlobalResource<FBlackVolumeTexture<PF_R8G8B8A8_UINT, 0>, FRenderResource::EInitPhase::Pre>();

class FBlackArrayTexture : public FTexture
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Create the texture RHI.
		FColorBulkData BulkData(0);

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2DArray(TEXT("BlackArrayTexture"), 1, 1, 1, PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetBulkData(&BulkData)
			.SetClassName(TEXT("FBlackArrayTexture"));

		TextureRHI = RHICreateTexture(Desc);

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Wrap, AM_Wrap, AM_Wrap);
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
	}

	virtual uint32 GetSizeX() const override { return 1; }
	virtual uint32 GetSizeY() const override { return 1; }
};

FTexture* GBlackArrayTexture = new TGlobalResource<FBlackArrayTexture, FRenderResource::EInitPhase::Pre>;

//
// FMipColorTexture implementation
//

/**
 * A texture that has a different solid color in each mip-level
 */
class FMipColorTexture : public FTexture
{
public:
	enum
	{
		NumMips = 12
	};
	static const FColor MipColors[NumMips];

	// FResource interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Create the texture RHI.
		int32 TextureSize = 1 << (NumMips - 1);

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("FMipColorTexture"), TextureSize, TextureSize, PF_B8G8R8A8)
			.SetNumMips(NumMips)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetClassName(TEXT("FMipColorTexture"));

		TextureRHI = RHICreateTexture(Desc);

		// Write the contents of the texture.
		uint32 DestStride;
		int32 Size = TextureSize;
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			FColor* DestBuffer = (FColor*)RHILockTexture2D(TextureRHI, MipIndex, RLM_WriteOnly, DestStride, false);
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					DestBuffer[X] = MipColors[NumMips - 1 - MipIndex];
				}
				DestBuffer += DestStride / sizeof(FColor);
			}
			RHIUnlockTexture2D(TextureRHI, MipIndex, false);
			Size >>= 1;
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Wrap, AM_Wrap, AM_Wrap);
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override
	{
		int32 TextureSize = 1 << (NumMips - 1);
		return TextureSize;
	}

	/** Returns the height of the texture in pixels. */
	// PVS-Studio notices that the implementation of GetSizeX is identical to this one
	// and warns us. In this case, it is intentional, so we disable the warning:
	virtual uint32 GetSizeY() const override //-V524
	{
		int32 TextureSize = 1 << (NumMips - 1);
		return TextureSize;
	}
};

const FColor FMipColorTexture::MipColors[NumMips] =
{
	FColor(80,  80,  80, 0),		// Mip  0: 1x1			(dark grey)
	FColor(200, 200, 200, 0),		// Mip  1: 2x2			(light grey)
	FColor(200, 200,   0, 0),		// Mip  2: 4x4			(medium yellow)
	FColor(255, 255,   0, 0),		// Mip  3: 8x8			(yellow)
	FColor(160, 255,  40, 0),		// Mip  4: 16x16		(light green)
	FColor(0, 255,   0, 0),		// Mip  5: 32x32		(green)
	FColor(0, 255, 200, 0),		// Mip  6: 64x64		(cyan)
	FColor(0, 170, 170, 0),		// Mip  7: 128x128		(light blue)
	FColor(60,  60, 255, 0),		// Mip  8: 256x256		(dark blue)
	FColor(255,   0, 255, 0),		// Mip  9: 512x512		(pink)
	FColor(255,   0,   0, 0),		// Mip 10: 1024x1024	(red)
	FColor(255, 130,   0, 0),		// Mip 11: 2048x2048	(orange)
};

FTexture* GMipColorTexture = new FMipColorTexture;
int32 GMipColorTextureMipLevels = FMipColorTexture::NumMips;

// 4: 8x8 cubemap resolution, shader needs to use the same value as preprocessing
const uint32 GDiffuseConvolveMipLevel = 4;

/** A solid color cube texture. */
class FSolidColorTextureCube : public FTexture
{
public:
	FSolidColorTextureCube(const FColor& InColor)
		: bInitToZero(false)
		, PixelFormat(PF_B8G8R8A8)
		, ColorData(InColor.DWColor())
	{}

	FSolidColorTextureCube(EPixelFormat InPixelFormat)
		: bInitToZero(true)
		, PixelFormat(InPixelFormat)
		, ColorData(0)
	{}

	// FRenderResource interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Create the texture RHI.
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::CreateCube(TEXT("SolidColorCube"), 1, PixelFormat)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetClassName(TEXT("FSolidColorTextureCube"));

		FTextureCubeRHIRef TextureCube = RHICreateTexture(Desc);
		TextureRHI = TextureCube;

		// Write the contents of the texture.
		for (uint32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			uint32 DestStride;
			void* DestBuffer = RHILockTextureCubeFace(TextureCube, FaceIndex, 0, 0, RLM_WriteOnly, DestStride, false);
			if (bInitToZero)
			{
				FMemory::Memzero(DestBuffer, GPixelFormats[PixelFormat].BlockBytes);
			}
			else
			{
				FMemory::Memcpy(DestBuffer, &ColorData, sizeof(ColorData));
			}
			RHIUnlockTextureCubeFace(TextureCube, FaceIndex, 0, 0, false);
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Wrap, AM_Wrap, AM_Wrap);
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
	}

	virtual uint32 GetSizeX() const override { return 1; }
	virtual uint32 GetSizeY() const override { return 1; }

private:
	const bool bInitToZero;
	const EPixelFormat PixelFormat;
	const uint32 ColorData;
};

/** A white cube texture. */
FTexture* GWhiteTextureCube = new TGlobalResource<FSolidColorTextureCube, FRenderResource::EInitPhase::Pre>(FColor::White);

/** A black cube texture. */
FTexture* GBlackTextureCube = new TGlobalResource<FSolidColorTextureCube, FRenderResource::EInitPhase::Pre>(FColor::Black);

/** A black cube texture. */
FTexture* GBlackTextureDepthCube = new TGlobalResource<FSolidColorTextureCube, FRenderResource::EInitPhase::Pre>(PF_ShadowDepth);

class FBlackCubeArrayTexture : public FTexture
{
public:
	// FResource interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		const TCHAR* Name = TEXT("BlackCubeArray");

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::CreateCubeArray(TEXT("BlackCubeArray"), 1, 1, PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetClassName(TEXT("FBlackCubeArrayTexture"));

		// Create the texture RHI.
		TextureRHI = RHICreateTexture(Desc);

		for (uint32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			uint32 DestStride;
			FColor* DestBuffer = (FColor*)RHILockTextureCubeFace(TextureRHI, FaceIndex, 0, 0, RLM_WriteOnly, DestStride, false);
			// Note: alpha is used by reflection environment to say how much of the foreground texture is visible, so 0 says it is completely invisible
			*DestBuffer = FColor(0, 0, 0, 0);
			RHIUnlockTextureCubeFace(TextureRHI, FaceIndex, 0, 0, false);
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Wrap, AM_Wrap, AM_Wrap);
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
	}

	virtual uint32 GetSizeX() const override { return 1; }
	virtual uint32 GetSizeY() const override { return 1; }
};
FTexture* GBlackCubeArrayTexture = new TGlobalResource<FBlackCubeArrayTexture, FRenderResource::EInitPhase::Pre>;

/**
 * A UINT 1x1 texture.
 */
template <EPixelFormat Format, uint32 R = 0, uint32 G = 0, uint32 B = 0, uint32 A = 0>
class FUintTexture : public FTextureWithSRV
{
public:
	// FResource interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("UintTexture"), 1, 1, Format)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetClassName(TEXT("FUintTexture"));

		TextureRHI = RHICreateTexture(Desc);

		// Write the contents of the texture.
		uint32 DestStride;
		void* DestBuffer = RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false);
		WriteData(DestBuffer);
		RHIUnlockTexture2D(TextureRHI, 0, false);

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Wrap, AM_Wrap, AM_Wrap);
		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);

		// Create a view of the texture
		ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(TextureRHI, 0u);
	}

	virtual uint32 GetSizeX() const override { return 1; }
	virtual uint32 GetSizeY() const override { return 1; }

protected:
	static int32 GetNumChannels()
	{
		return GPixelFormats[Format].NumComponents;
	}

	static int32 GetBytesPerChannel()
	{
		return GPixelFormats[Format].BlockBytes / GPixelFormats[Format].NumComponents;
	}

	template<typename T>
	static void DoWriteData(T* DataPtr)
	{
		T Values[] = { R, G, B, A };
		for (int32 i = 0; i < GetNumChannels(); ++i)
		{
			DataPtr[i] = Values[i];
		}
	}

	static void WriteData(void* DataPtr)
	{
		switch (GetBytesPerChannel())
		{
		case 1:
			DoWriteData((uint8*)DataPtr);
			return;
		case 2:
			DoWriteData((uint16*)DataPtr);
			return;
		case 4:
			DoWriteData((uint32*)DataPtr);
			return;
		}
		// Unsupported format
		check(0);
	}
};

FTexture* GBlackUintTexture = new TGlobalResource<FUintTexture<PF_R32G32B32A32_UINT>, FRenderResource::EInitPhase::Pre>;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FNullColorVertexBuffer

FNullColorVertexBuffer::FNullColorVertexBuffer() = default;
FNullColorVertexBuffer::~FNullColorVertexBuffer() = default;

void FNullColorVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo(TEXT("FNullColorVertexBuffer"));

	VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(uint32) * 4, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
	uint32* Vertices = (uint32*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(uint32) * 4, RLM_WriteOnly);
	Vertices[0] = FColor(255, 255, 255, 255).DWColor();
	Vertices[1] = FColor(255, 255, 255, 255).DWColor();
	Vertices[2] = FColor(255, 255, 255, 255).DWColor();
	Vertices[3] = FColor(255, 255, 255, 255).DWColor();
	RHICmdList.UnlockBuffer(VertexBufferRHI);
	VertexBufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(FColor), PF_R8G8B8A8);
}

void FNullColorVertexBuffer::ReleaseRHI()
{
	VertexBufferSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

/** The global null color vertex buffer, which is set with a stride of 0 on meshes without a color component. */
TGlobalResource<FNullColorVertexBuffer, FRenderResource::EInitPhase::Pre> GNullColorVertexBuffer;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FNullVertexBuffer

FNullVertexBuffer::FNullVertexBuffer() = default;
FNullVertexBuffer::~FNullVertexBuffer() = default;

void FNullVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo(TEXT("FNullVertexBuffer"));
	VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FVector3f), BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
	FVector3f* LockedData = (FVector3f*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector3f), RLM_WriteOnly);
	*LockedData = FVector3f(0.0f);
	RHICmdList.UnlockBuffer(VertexBufferRHI);

	VertexBufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(FColor), PF_R8G8B8A8);
}

void FNullVertexBuffer::ReleaseRHI()
{
	VertexBufferSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

/** The global null vertex buffer, which is set with a stride of 0 on meshes */
TGlobalResource<FNullVertexBuffer, FRenderResource::EInitPhase::Pre> GNullVertexBuffer;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FScreenSpaceVertexBuffer

void FScreenSpaceVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo(TEXT("FScreenSpaceVertexBuffer"));
	VertexBufferRHI = RHICmdList.CreateVertexBuffer(sizeof(FVector2f) * 4, BUF_Static, CreateInfo);
	void* VoidPtr = RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector2f) * 4, RLM_WriteOnly);
	static const FVector2f Vertices[4] =
	{
		FVector2f(-1,-1),
		FVector2f(-1,+1),
		FVector2f(+1,-1),
		FVector2f(+1,+1),
	};
	FMemory::Memcpy(VoidPtr, Vertices, sizeof(FVector2f) * 4);
	RHICmdList.UnlockBuffer(VertexBufferRHI);
}

TGlobalResource<FScreenSpaceVertexBuffer, FRenderResource::EInitPhase::Pre> GScreenSpaceVertexBuffer;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FTileVertexDeclaration

FTileVertexDeclaration::FTileVertexDeclaration() = default;
FTileVertexDeclaration::~FTileVertexDeclaration() = default;

void FTileVertexDeclaration::InitRHI(FRHICommandListBase& RHICmdList)
{
	FVertexDeclarationElementList Elements;
	uint16 Stride = sizeof(FVector2f);
	Elements.Add(FVertexElement(0, 0, VET_Float2, 0, Stride, false));
	VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
}

void FTileVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}

TGlobalResource<FTileVertexDeclaration, FRenderResource::EInitPhase::Pre> GTileVertexDeclaration;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FCubeIndexBuffer

void FCubeIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo(TEXT("FCubeIndexBuffer"));
	IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint16), sizeof(uint16) * NUM_CUBE_VERTICES, BUF_Static, CreateInfo);
	void* VoidPtr = RHICmdList.LockBuffer(IndexBufferRHI, 0, sizeof(uint16) * NUM_CUBE_VERTICES, RLM_WriteOnly);
	FMemory::Memcpy(VoidPtr, GCubeIndices, NUM_CUBE_VERTICES * sizeof(uint16));
	RHICmdList.UnlockBuffer(IndexBufferRHI);
}

TGlobalResource<FCubeIndexBuffer, FRenderResource::EInitPhase::Pre> GCubeIndexBuffer;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FTwoTrianglesIndexBuffer

void FTwoTrianglesIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo(TEXT("FTwoTrianglesIndexBuffer"));
	IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint16), sizeof(uint16) * 6, BUF_Static, CreateInfo);
	void* VoidPtr = RHICmdList.LockBuffer(IndexBufferRHI, 0, sizeof(uint16) * 6, RLM_WriteOnly);
	static const uint16 Indices[] = { 0, 1, 3, 0, 3, 2 };
	FMemory::Memcpy(VoidPtr, Indices, 6 * sizeof(uint16));
	RHICmdList.UnlockBuffer(IndexBufferRHI);
}

TGlobalResource<FTwoTrianglesIndexBuffer, FRenderResource::EInitPhase::Pre> GTwoTrianglesIndexBuffer;

/*------------------------------------------------------------------------------
	FGlobalDynamicVertexBuffer implementation.
------------------------------------------------------------------------------*/

/**
 * An individual dynamic vertex buffer.
 */
template <typename BufferType>
class TDynamicBuffer final : public BufferType
{
public:
	/** The aligned size of all dynamic vertex buffers. */
	enum { ALIGNMENT = (1 << 16) }; // 64KB
	/** Pointer to the vertex buffer mapped in main memory. */
	uint8* MappedBuffer;
	/** Size of the vertex buffer in bytes. */
	uint32 BufferSize;
	/** Number of bytes currently allocated from the buffer. */
	uint32 AllocatedByteCount;
	/** Stride of the buffer in bytes. */
	uint32 Stride;
	/** Last render thread frame this resource was used in. */
	uint64 LastUsedFrame = 0;

	/** Default constructor. */
	explicit TDynamicBuffer(uint32 InMinBufferSize, uint32 InStride)
		: MappedBuffer(NULL)
		, BufferSize(FMath::Max<uint32>(Align(InMinBufferSize, ALIGNMENT), ALIGNMENT))
		, AllocatedByteCount(0)
		, Stride(InStride)
	{
	}

	/**
	 * Locks the vertex buffer so it may be written to.
	 */
	void Lock(FRHICommandListBase& RHICmdList)
	{
		check(MappedBuffer == NULL);
		check(AllocatedByteCount == 0);
		check(IsValidRef(BufferType::GetRHI()));
		MappedBuffer = (uint8*)RHICmdList.LockBuffer(BufferType::GetRHI(), 0, BufferSize, RLM_WriteOnly);
	}

	/**
	 * Unocks the buffer so the GPU may read from it.
	 */
	void Unlock(FRHICommandListBase& RHICmdList)
	{
		check(MappedBuffer != NULL);
		check(IsValidRef(BufferType::GetRHI()));
		RHICmdList.UnlockBuffer(BufferType::GetRHI());
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
	}

	// FRenderResource interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		check(!IsValidRef(BufferType::GetRHI()));
		FRHIResourceCreateInfo CreateInfo(TEXT("FDynamicBuffer"));
		BufferType::SetRHI(RHICmdList.CreateBuffer(BufferSize, Stride == 0 ? EBufferUsageFlags::VertexBuffer : EBufferUsageFlags::IndexBuffer, Stride, ERHIAccess::VertexOrIndexBuffer, CreateInfo));
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
	}

	virtual void ReleaseRHI() override
	{
		BufferType::ReleaseRHI();
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
	}
};

/**
 * A pool of dynamic buffers.
 */
template <typename DynamicBufferType>
struct TDynamicBufferPool : public FRenderResource
{
	UE::FMutex Mutex;
	TArray<DynamicBufferType*> LiveList;
	TArray<DynamicBufferType*> FreeList;
	TArray<DynamicBufferType*> LockList;
	TArray<DynamicBufferType*> ReclaimList;
	std::atomic_uint32_t TotalAllocatedMemory{0};
	uint32 CurrentCycle = 0;

	DynamicBufferType* Acquire(FRHICommandListBase& RHICmdList, uint32 SizeInBytes, uint32 Stride)
	{
		const uint32 MinimumBufferSize = 65536u;
		SizeInBytes = FMath::Max(SizeInBytes, MinimumBufferSize);

		DynamicBufferType* FoundBuffer = nullptr;
		bool bInitializeBuffer = false;

		{
			UE::TScopeLock Lock(Mutex);

			// Traverse the free list like a stack, starting from the top, so recently reclaimed items are allocated first.
			for (int32 Index = FreeList.Num() - 1; Index >= 0; --Index)
			{
				DynamicBufferType* Buffer = FreeList[Index];

				if (SizeInBytes <= Buffer->BufferSize && Buffer->Stride == Stride)
				{
					FreeList.RemoveAt(Index, 1, EAllowShrinking::No);
					FoundBuffer = Buffer;
					break;
				}
			}

			if (!FoundBuffer)
			{
				FoundBuffer = new DynamicBufferType(SizeInBytes, Stride);
				LiveList.Emplace(FoundBuffer);
				bInitializeBuffer = true;
			}

			check(FoundBuffer);
		}

		if (IsRenderAlarmLoggingEnabled())
		{
			UE_LOG(LogRendererCore, Warning, TEXT("FGlobalDynamicVertexBuffer::Allocate(%u), will have allocated %u total this frame"), SizeInBytes, TotalAllocatedMemory.load());
		}

		if (bInitializeBuffer)
		{
			FoundBuffer->InitResource(RHICmdList);
			TotalAllocatedMemory += FoundBuffer->BufferSize;
		}

		FoundBuffer->Lock(RHICmdList);
		FoundBuffer->LastUsedFrame = GFrameCounterRenderThread;

		return FoundBuffer;
	}

	void Forfeit(FRHICommandListBase& RHICmdList, TConstArrayView<DynamicBufferType*> BuffersToForfeit)
	{
		if (!BuffersToForfeit.IsEmpty())
		{
			for (DynamicBufferType* Buffer : BuffersToForfeit)
			{
				Buffer->Unlock(RHICmdList);
			}

			UE::TScopeLock Lock(Mutex);
			ReclaimList.Append(BuffersToForfeit);
		}
	}

	void GarbageCollect()
	{
		UE::TScopeLock Lock(Mutex);
		FreeList.Append(ReclaimList);
		ReclaimList.Reset();

		for (int32 Index = 0; Index < LiveList.Num(); ++Index)
		{
			DynamicBufferType* Buffer = LiveList[Index];

			if (GGlobalBufferNumFramesUnusedThreshold > 0 && Buffer->LastUsedFrame + GGlobalBufferNumFramesUnusedThreshold <= GFrameCounterRenderThread)
			{
				TotalAllocatedMemory -= Buffer->BufferSize;
				Buffer->ReleaseResource();
				LiveList.RemoveAt(Index, 1, EAllowShrinking::No);
				FreeList.Remove(Buffer);
				delete Buffer;
			}
		}
	}

	bool IsRenderAlarmLoggingEnabled() const
	{
		return GMaxVertexBytesAllocatedPerFrame > 0 && TotalAllocatedMemory >= (size_t)GMaxVertexBytesAllocatedPerFrame;
	}

private:
	void ReleaseRHI() override
	{
		check(LockList.IsEmpty());
		check(FreeList.Num() == LiveList.Num());

		for (DynamicBufferType* Buffer : LiveList)
		{
			TotalAllocatedMemory -= Buffer->BufferSize;
			Buffer->ReleaseResource();
			delete Buffer;
		}
		LiveList.Empty();
		FreeList.Empty();
	}
};

static TGlobalResource<TDynamicBufferPool<FDynamicVertexBuffer>, FRenderResource::EInitPhase::Pre> GDynamicVertexBufferPool;

FGlobalDynamicVertexBuffer::FAllocation FGlobalDynamicVertexBuffer::Allocate(uint32 SizeInBytes)
{
	checkf(RHICmdList, TEXT("FGlobalDynamicVertexBuffer was not initialized prior to calling Allocate."));

	FAllocation Allocation;

	if (VertexBuffers.IsEmpty() || VertexBuffers.Last()->AllocatedByteCount + SizeInBytes > VertexBuffers.Last()->BufferSize)
	{
		VertexBuffers.Emplace(GDynamicVertexBufferPool.Acquire(*RHICmdList, SizeInBytes, 0));
	}

	FDynamicVertexBuffer* VertexBuffer = VertexBuffers.Last();

	checkf(VertexBuffer->AllocatedByteCount + SizeInBytes <= VertexBuffer->BufferSize, TEXT("Global vertex buffer allocation failed: BufferSize=%d AllocatedByteCount=%d SizeInBytes=%d"), VertexBuffer->BufferSize, VertexBuffer->AllocatedByteCount, SizeInBytes);
	Allocation.Buffer = VertexBuffer->MappedBuffer + VertexBuffer->AllocatedByteCount;
	Allocation.VertexBuffer = VertexBuffer;
	Allocation.VertexOffset = VertexBuffer->AllocatedByteCount;
	VertexBuffer->AllocatedByteCount += SizeInBytes;
	return Allocation;
}

bool FGlobalDynamicVertexBuffer::IsRenderAlarmLoggingEnabled() const
{
	return GDynamicVertexBufferPool.IsRenderAlarmLoggingEnabled();
}

void FGlobalDynamicVertexBuffer::Commit()
{
	if (RHICmdList)
	{
		GDynamicVertexBufferPool.Forfeit(*RHICmdList, VertexBuffers);
		VertexBuffers.Reset();
	}
}

static TGlobalResource<TDynamicBufferPool<FDynamicIndexBuffer>, FRenderResource::EInitPhase::Pre> GDynamicIndexBufferPool;

FGlobalDynamicIndexBuffer::FAllocation FGlobalDynamicIndexBuffer::Allocate(uint32 NumIndices, uint32 IndexStride)
{
	checkf(RHICmdList, TEXT("FGlobalDynamicIndexBuffer was not initialized prior to calling Allocate."));

	FAllocation Allocation;

	if (IndexStride != 2 && IndexStride != 4)
	{
		return Allocation;
	}

	const uint32 SizeInBytes = NumIndices * IndexStride;

	TArray<FDynamicIndexBuffer*>& IndexBuffers = (IndexStride == 2)
		? IndexBuffers16
		: IndexBuffers32;

	if (IndexBuffers.IsEmpty() || IndexBuffers.Last()->AllocatedByteCount + SizeInBytes > IndexBuffers.Last()->BufferSize)
	{
		IndexBuffers.Emplace(GDynamicIndexBufferPool.Acquire(*RHICmdList, SizeInBytes, IndexStride));
	}

	FDynamicIndexBuffer* IndexBuffer = IndexBuffers.Last();

	checkf(IndexBuffer->AllocatedByteCount + SizeInBytes <= IndexBuffer->BufferSize, TEXT("Global index buffer allocation failed: BufferSize=%d BufferStride=%d AllocatedByteCount=%d SizeInBytes=%d"), IndexBuffer->BufferSize, IndexBuffer->Stride, IndexBuffer->AllocatedByteCount, SizeInBytes);
	Allocation.Buffer = IndexBuffer->MappedBuffer + IndexBuffer->AllocatedByteCount;
	Allocation.IndexBuffer = IndexBuffer;
	Allocation.FirstIndex = IndexBuffer->AllocatedByteCount / IndexStride;
	IndexBuffer->AllocatedByteCount += SizeInBytes;
	return Allocation;
}

void FGlobalDynamicIndexBuffer::Commit()
{
	if (RHICmdList)
	{
		GDynamicIndexBufferPool.Forfeit(*RHICmdList, IndexBuffers16);
		GDynamicIndexBufferPool.Forfeit(*RHICmdList, IndexBuffers32);
		IndexBuffers16.Reset();
		IndexBuffers32.Reset();
	}
}

namespace GlobalDynamicBuffer
{
	void GarbageCollect()
	{
		GDynamicVertexBufferPool.GarbageCollect();
		GDynamicIndexBufferPool.GarbageCollect();
	}
}