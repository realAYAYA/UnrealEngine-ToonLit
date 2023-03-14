// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "HAL/IConsoleManager.h"
#include "RHIDefinitions.h"
#include "RHICommandList.h"


static inline bool IsDepthOrStencilFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_D24:
	case PF_DepthStencil:
	case PF_X24_G8:
	case PF_ShadowDepth:
	case PF_R32_FLOAT:
		return true;

	default:
		break;
	}

	return false;
}

static inline bool IsStencilFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_DepthStencil:
	case PF_X24_G8:
		return true;

	default:
		break;
	}

	return false;
}

static inline bool IsBlockCompressedFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5:
	case PF_BC4:
	case PF_BC5:
	case PF_BC6H:
	case PF_BC7:
		return true;
	}

	return false;
}

static inline EPixelFormat GetBlockCompressedFormatUAVAliasFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_DXT1:
	case PF_BC4:
		return PF_R32G32_UINT;

	case PF_DXT3:
	case PF_DXT5:
	case PF_BC5:
	case PF_BC6H:
	case PF_BC7:
		return PF_R32G32B32A32_UINT;
	}

	return Format;
}

static bool IsFloatFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_A32B32G32R32F:
	case PF_FloatRGB:
	case PF_FloatRGBA:
	case PF_R32_FLOAT:
	case PF_G16R16F:
	case PF_G16R16F_FILTER:
	case PF_G32R32F:
	case PF_R16F:
	case PF_R16F_FILTER:
	case PF_FloatR11G11B10:
		return true;

	default:
		break;
	}
	return false;
}

static bool IsUnormFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_R5G6B5_UNORM:
	case PF_R16G16B16A16_UNORM:
	case PF_B5G5R5A1_UNORM:
		return true;

	default:
		break;
	}
	return false;
}

static bool IsSnormFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_R8G8B8A8_SNORM:
	case PF_R16G16B16A16_SNORM:
	case PF_G16R16_SNORM:
		return true;

	default:
		break;
	}
	return false;
}

static bool IsUintFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_R32_UINT:
	case PF_R16_UINT:
	case PF_R16G16B16A16_UINT:
	case PF_R32G32B32A32_UINT:
	case PF_R16G16_UINT:
	case PF_R8_UINT:
	case PF_R8G8B8A8_UINT:
	case PF_R32G32_UINT:
		return true;

	default:
		break;
	}
	return false;
}

static bool IsSintFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_R32_SINT:
	case PF_R16_SINT:
	case PF_R16G16B16A16_SINT:
		return true;

	default:
		break;
	}
	return false;
}

/** Get the best default resource state for the given texture creation flags */
extern RHI_API ERHIAccess RHIGetDefaultResourceState(ETextureCreateFlags InUsage, bool bInHasInitialData);

/** Get the best default resource state for the given buffer creation flags */
extern RHI_API ERHIAccess RHIGetDefaultResourceState(EBufferUsageFlags InUsage, bool bInHasInitialData);

/** Encapsulates a GPU read/write texture 2D with its UAV and SRV. */
struct FTextureRWBuffer
{
	FTextureRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes = 0;

	FTextureRWBuffer() = default;

	~FTextureRWBuffer()
	{
		Release();
	}

	static constexpr ETextureCreateFlags DefaultTextureInitFlag = TexCreate_ShaderResource | TexCreate_UAV;

	void Initialize2D(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 SizeX, uint32 SizeY, EPixelFormat Format, ETextureCreateFlags Flags = DefaultTextureInitFlag)
	{
		NumBytes = SizeX * SizeY * BytesPerElement;

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(InDebugName, SizeX, SizeY, Format)
			.SetFlags(Flags);

		Buffer = RHICreateTexture(Desc);
		UAV = RHICreateUnorderedAccessView(Buffer, 0);
		SRV = RHICreateShaderResourceView(Buffer, 0);
	}

	void Initialize3D(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 SizeX, uint32 SizeY, uint32 SizeZ, EPixelFormat Format, ETextureCreateFlags Flags = DefaultTextureInitFlag)
	{
		NumBytes = SizeX * SizeY * SizeZ * BytesPerElement;

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create3D(InDebugName, SizeX, SizeY, SizeZ, Format)
			.SetFlags(Flags);

		Buffer = RHICreateTexture(Desc);
		UAV = RHICreateUnorderedAccessView(Buffer, 0);
		SRV = RHICreateShaderResourceView(Buffer, 0);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}
};

struct UE_DEPRECATED(5.1, "FTextureRWBuffer should be used instead of FTextureRWBuffer2D.") FTextureRWBuffer2D;
struct FTextureRWBuffer2D : public FTextureRWBuffer
{
	UE_DEPRECATED(5.1, "FTextureRWBuffer::Initialize2D should be used instead of FTextureRWBuffer2D::Initialize.")
	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 SizeX, uint32 SizeY, EPixelFormat Format, ETextureCreateFlags Flags = DefaultTextureInitFlag)
	{
		Initialize2D(InDebugName, BytesPerElement, SizeX, SizeY, Format, Flags);
	}

	UE_DEPRECATED(5.0, "AcquireTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void AcquireTransientResource() {}

	UE_DEPRECATED(5.0, "DiscardTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void DiscardTransientResource() {}
};

struct UE_DEPRECATED(5.1, "FTextureRWBuffer should be used instead of FTextureRWBuffer3D.") FTextureRWBuffer3D;
struct FTextureRWBuffer3D : public FTextureRWBuffer
{
	UE_DEPRECATED(5.1, "FTextureRWBuffer::Initialize3D should be used instead of FTextureRWBuffer3D::Initialize.")
	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 SizeX, uint32 SizeY, uint32 SizeZ, EPixelFormat Format, ETextureCreateFlags Flags = DefaultTextureInitFlag)
	{
		Initialize3D(InDebugName, BytesPerElement, SizeX, SizeY, SizeZ, Format, Flags);
	}

	UE_DEPRECATED(5.0, "AcquireTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void AcquireTransientResource() {}

	UE_DEPRECATED(5.0, "DiscardTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void DiscardTransientResource() {}
};

/** Encapsulates a GPU read/write buffer with its UAV and SRV. */
struct FRWBuffer
{
	FBufferRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FRWBuffer()
		: NumBytes(0)
	{}

	FRWBuffer(FRWBuffer&& Other)
		: Buffer(MoveTemp(Other.Buffer))
		, UAV(MoveTemp(Other.UAV))
		, SRV(MoveTemp(Other.SRV))
		, NumBytes(Other.NumBytes)
	{
		Other.NumBytes = 0;
	}

	FRWBuffer(const FRWBuffer& Other)
		: Buffer(Other.Buffer)
		, UAV(Other.UAV)
		, SRV(Other.SRV)
		, NumBytes(Other.NumBytes)
	{
	}

	FRWBuffer& operator=(FRWBuffer&& Other)
	{
		Buffer = MoveTemp(Other.Buffer);
		UAV = MoveTemp(Other.UAV);
		SRV = MoveTemp(Other.SRV);
		NumBytes = Other.NumBytes;
		Other.NumBytes = 0;

		return *this;
	}

	FRWBuffer& operator=(const FRWBuffer& Other)
	{
		Buffer = Other.Buffer;
		UAV = Other.UAV;
		SRV = Other.SRV;
		NumBytes = Other.NumBytes;

		return *this;
	}

	~FRWBuffer()
	{
		Release();
	}

	// @param AdditionalUsage passed down to RHICreateVertexBuffer(), get combined with "BUF_UnorderedAccess | BUF_ShaderResource" e.g. BUF_Static
	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, ERHIAccess InResourceState, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface *InResourceArray = nullptr)
	{
		// Provide a debug name if using Fast VRAM so the allocators diagnostics will work
		ensure(!(EnumHasAnyFlags(AdditionalUsage, BUF_FastVRAM) && !InDebugName));
		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo(InDebugName);
		CreateInfo.ResourceArray = InResourceArray;
		Buffer = RHICreateVertexBuffer(NumBytes, BUF_UnorderedAccess | BUF_ShaderResource | AdditionalUsage, InResourceState, CreateInfo);
		UAV = RHICreateUnorderedAccessView(Buffer, UE_PIXELFORMAT_TO_UINT8(Format));
		SRV = RHICreateShaderResourceView(Buffer, BytesPerElement, UE_PIXELFORMAT_TO_UINT8(Format));
	}

	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface* InResourceArray = nullptr)
	{
		Initialize(InDebugName, BytesPerElement, NumElements, Format, ERHIAccess::UAVCompute, AdditionalUsage, InResourceArray);
	}

	UE_DEPRECATED(5.0, "AcquireTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void AcquireTransientResource() {}

	UE_DEPRECATED(5.0, "DiscardTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void DiscardTransientResource() {}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read only texture 2D with its SRV. */
struct FTextureReadBuffer2D
{
	FTexture2DRHIRef Buffer;	
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FTextureReadBuffer2D()
		: NumBytes(0)
	{}

	~FTextureReadBuffer2D()
	{
		Release();
	}

	const static ETextureCreateFlags DefaultTextureInitFlag = ETextureCreateFlags::ShaderResource;
	void Initialize(const TCHAR* InDebugName, const uint32 BytesPerElement, const uint32 SizeX, const uint32 SizeY, const EPixelFormat Format, ETextureCreateFlags Flags = DefaultTextureInitFlag, FResourceBulkDataInterface* InBulkData = nullptr)
	{
		NumBytes = SizeX * SizeY * BytesPerElement;

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(InDebugName, SizeX, SizeY, Format)
			.SetFlags(Flags)
			.SetBulkData(InBulkData);

		Buffer = RHICreateTexture(Desc);
		
		SRV = RHICreateShaderResourceView(Buffer, 0);
	}

	UE_DEPRECATED(5.0, "AcquireTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void AcquireTransientResource() {}

	UE_DEPRECATED(5.0, "DiscardTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void DiscardTransientResource() {}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();		
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read buffer with its SRV. */
struct FReadBuffer
{
	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FReadBuffer(): NumBytes(0) {}

	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None, FResourceArrayInterface* InResourceArray = nullptr)
	{
		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo(InDebugName);
		CreateInfo.ResourceArray = InResourceArray;
		Buffer = RHICreateVertexBuffer(NumBytes, BUF_ShaderResource | AdditionalUsage, ERHIAccess::SRVMask, CreateInfo);
		SRV = RHICreateShaderResourceView(Buffer, BytesPerElement, UE_PIXELFORMAT_TO_UINT8(Format));
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read/write structured buffer with its UAV and SRV. */
struct FRWBufferStructured
{
	FBufferRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FRWBufferStructured(): NumBytes(0) {}

	~FRWBufferStructured()
	{
		Release();
	}

	void Initialize(const TCHAR* InDebugName, uint32 BytesPerElement, uint32 NumElements, EBufferUsageFlags AdditionalUsage = BUF_None, bool bUseUavCounter = false, bool bAppendBuffer = false, ERHIAccess InitialState = ERHIAccess::UAVMask)
	{
		check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5 || GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1);
		// Provide a debug name if using Fast VRAM so the allocators diagnostics will work
		ensure(!(EnumHasAnyFlags(AdditionalUsage, BUF_FastVRAM) && !InDebugName));

		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo(InDebugName);
		Buffer = RHICreateStructuredBuffer(BytesPerElement, NumBytes, BUF_UnorderedAccess | BUF_ShaderResource | AdditionalUsage, InitialState, CreateInfo);
		UAV = RHICreateUnorderedAccessView(Buffer, bUseUavCounter, bAppendBuffer);
		SRV = RHICreateShaderResourceView(Buffer);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}

	UE_DEPRECATED(5.0, "AcquireTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void AcquireTransientResource() {}

	UE_DEPRECATED(5.0, "DiscardTransientResource is deprecated. Transient resources are allocated through IRHITransientResourceAllocator instead.")
	void DiscardTransientResource() {}
};

struct FByteAddressBuffer
{
	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FByteAddressBuffer(): NumBytes(0) {}

	void Initialize(const TCHAR* InDebugName, uint32 InNumBytes, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		NumBytes = InNumBytes;
		check( NumBytes % 4 == 0 );
		FRHIResourceCreateInfo CreateInfo(InDebugName);
		Buffer = RHICreateStructuredBuffer(4, NumBytes, BUF_ShaderResource | BUF_ByteAddressBuffer | AdditionalUsage, ERHIAccess::SRVMask, CreateInfo);
		SRV = RHICreateShaderResourceView(Buffer);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read/write ByteAddress buffer with its UAV and SRV. */
struct FRWByteAddressBuffer : public FByteAddressBuffer
{
	FUnorderedAccessViewRHIRef UAV;

	void Initialize(const TCHAR* DebugName, uint32 InNumBytes, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		FByteAddressBuffer::Initialize(DebugName, InNumBytes, BUF_UnorderedAccess | AdditionalUsage);
		UAV = RHICreateUnorderedAccessView(Buffer, false, false);
	}

	void Release()
	{
		FByteAddressBuffer::Release();
		UAV.SafeRelease();
	}
};

struct FDynamicReadBuffer : public FReadBuffer
{
	/** Pointer to the vertex buffer mapped in main memory. */
	uint8* MappedBuffer;

	/** Default constructor. */
	FDynamicReadBuffer()
		: MappedBuffer(nullptr)
	{
	}

	virtual ~FDynamicReadBuffer()
	{
		Release();
	}

	virtual void Initialize(const TCHAR* DebugName, uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, EBufferUsageFlags AdditionalUsage = BUF_None)
	{
		ensure(
			EnumHasAnyFlags(AdditionalUsage, BUF_Dynamic | BUF_Volatile | BUF_Static) &&					// buffer should be Dynamic or Volatile or Static
			EnumHasAnyFlags(AdditionalUsage, BUF_Dynamic) != EnumHasAnyFlags(AdditionalUsage, BUF_Volatile) // buffer should not be both
			);

		FReadBuffer::Initialize(DebugName, BytesPerElement, NumElements, Format, AdditionalUsage);
	}

	/**
	* Locks the vertex buffer so it may be written to.
	*/
	void Lock()
	{
		check(MappedBuffer == nullptr);
		check(IsValidRef(Buffer));
		MappedBuffer = (uint8*)RHILockBuffer(Buffer, 0, NumBytes, RLM_WriteOnly);
	}

	/**
	* Unocks the buffer so the GPU may read from it.
	*/
	void Unlock()
	{
		check(MappedBuffer);
		check(IsValidRef(Buffer));
		RHIUnlockBuffer(Buffer);
		MappedBuffer = nullptr;
	}
};

/**
 * Convert the ESimpleRenderTargetMode into usable values 
 * @todo: Can we easily put this into a .cpp somewhere?
 */
inline void DecodeRenderTargetMode(ESimpleRenderTargetMode Mode, ERenderTargetLoadAction& ColorLoadAction, ERenderTargetStoreAction& ColorStoreAction, ERenderTargetLoadAction& DepthLoadAction, ERenderTargetStoreAction& DepthStoreAction, ERenderTargetLoadAction& StencilLoadAction, ERenderTargetStoreAction& StencilStoreAction, FExclusiveDepthStencil DepthStencilUsage)
{
	// set defaults
	ColorStoreAction = ERenderTargetStoreAction::EStore;
	DepthStoreAction = ERenderTargetStoreAction::EStore;
	StencilStoreAction = ERenderTargetStoreAction::EStore;

	switch (Mode)
	{
	case ESimpleRenderTargetMode::EExistingColorAndDepth:
		ColorLoadAction = ERenderTargetLoadAction::ELoad;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		break;
	case ESimpleRenderTargetMode::EUninitializedColorAndDepth:
		ColorLoadAction = ERenderTargetLoadAction::ENoAction;
		DepthLoadAction = ERenderTargetLoadAction::ENoAction;
		break;
	case ESimpleRenderTargetMode::EUninitializedColorExistingDepth:
		ColorLoadAction = ERenderTargetLoadAction::ENoAction;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		break;
	case ESimpleRenderTargetMode::EUninitializedColorClearDepth:
		ColorLoadAction = ERenderTargetLoadAction::ENoAction;
		DepthLoadAction = ERenderTargetLoadAction::EClear;
		break;
	case ESimpleRenderTargetMode::EClearColorExistingDepth:
		ColorLoadAction = ERenderTargetLoadAction::EClear;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		break;
	case ESimpleRenderTargetMode::EClearColorAndDepth:
		ColorLoadAction = ERenderTargetLoadAction::EClear;
		DepthLoadAction = ERenderTargetLoadAction::EClear;
		break;
	case ESimpleRenderTargetMode::EExistingContents_NoDepthStore:
		ColorLoadAction = ERenderTargetLoadAction::ELoad;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		DepthStoreAction = ERenderTargetStoreAction::ENoAction;
		break;
	case ESimpleRenderTargetMode::EExistingColorAndClearDepth:
		ColorLoadAction = ERenderTargetLoadAction::ELoad;
		DepthLoadAction = ERenderTargetLoadAction::EClear;
		break;
	case ESimpleRenderTargetMode::EExistingColorAndDepthAndClearStencil:
		ColorLoadAction = ERenderTargetLoadAction::ELoad;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		break;
	default:
		UE_LOG(LogRHI, Fatal, TEXT("Using a ESimpleRenderTargetMode that wasn't decoded in DecodeRenderTargetMode [value = %d]"), (int32)Mode);
	}
	
	StencilLoadAction = DepthLoadAction;
	
	if (!DepthStencilUsage.IsUsingDepth())
	{
		DepthLoadAction = ERenderTargetLoadAction::ENoAction;
	}
	
	//if we aren't writing to depth, there's no reason to store it back out again.  Should save some bandwidth on mobile platforms.
	if (!DepthStencilUsage.IsDepthWrite())
	{
		DepthStoreAction = ERenderTargetStoreAction::ENoAction;
	}
	
	if (!DepthStencilUsage.IsUsingStencil())
	{
		StencilLoadAction = ERenderTargetLoadAction::ENoAction;
	}
	
	//if we aren't writing to stencil, there's no reason to store it back out again.  Should save some bandwidth on mobile platforms.
	if (!DepthStencilUsage.IsStencilWrite())
	{
		StencilStoreAction = ERenderTargetStoreAction::ENoAction;
	}
}

inline void TransitionRenderPassTargets(FRHICommandList& RHICmdList, const FRHIRenderPassInfo& RPInfo)
{
	FRHITransitionInfo Transitions[MaxSimultaneousRenderTargets];
	int32 TransitionIndex = 0;
	uint32 NumColorRenderTargets = RPInfo.GetNumColorRenderTargets();
	for (uint32 Index = 0; Index < NumColorRenderTargets; Index++)
	{
		const FRHIRenderPassInfo::FColorEntry& ColorRenderTarget = RPInfo.ColorRenderTargets[Index];
		if (ColorRenderTarget.RenderTarget != nullptr)
		{
			Transitions[TransitionIndex] = FRHITransitionInfo(ColorRenderTarget.RenderTarget, ERHIAccess::Unknown, ERHIAccess::RTV);
			TransitionIndex++;
		}
	}

	const FRHIRenderPassInfo::FDepthStencilEntry& DepthStencilTarget = RPInfo.DepthStencilRenderTarget;
	if (DepthStencilTarget.DepthStencilTarget != nullptr && (RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsAnyWrite()))
	{
		RHICmdList.Transition(FRHITransitionInfo(DepthStencilTarget.DepthStencilTarget, ERHIAccess::Unknown, ERHIAccess::DSVRead | ERHIAccess::DSVWrite));
	}

	RHICmdList.Transition(MakeArrayView(Transitions, TransitionIndex));
}

/**
 * Creates 1 or 2 textures with the same dimensions/format.
 * If the RHI supports textures that can be used as both shader resources and render targets,
 * and bForceSeparateTargetAndShaderResource=false, then a single texture is created.
 * Otherwise two textures are created, one of them usable as a shader resource and resolve target, and one of them usable as a render target.
 * Two texture references are always returned, but they may reference the same texture.
 * If two different textures are returned, the render-target texture must be manually copied to the shader-resource texture.
 */
UE_DEPRECATED(5.1, "RHICreateTargetableShaderResource2D is deprecated. RHICreateTexture should be used instead.")
RHI_API void RHICreateTargetableShaderResource2D(
	uint32 SizeX,
	uint32 SizeY,
	uint8 Format,
	uint32 NumMips,
	ETextureCreateFlags Flags,
	ETextureCreateFlags TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	bool bForceSharedTargetAndShaderResource,
	const FRHIResourceCreateInfo& CreateInfo,
	FTextureRHIRef& OutTargetableTexture,
	FTextureRHIRef& OutShaderResourceTexture,
	uint32 NumSamples = 1);

UE_DEPRECATED(5.1, "RHICreateTargetableShaderResource2D is deprecated. RHICreateTexture should be used instead.")
RHI_API void RHICreateTargetableShaderResource2D(
	uint32 SizeX,
	uint32 SizeY,
	uint8 Format,
	uint32 NumMips,
	ETextureCreateFlags Flags,
	ETextureCreateFlags TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	const FRHIResourceCreateInfo& CreateInfo,
	FTextureRHIRef& OutTargetableTexture,
	FTextureRHIRef& OutShaderResourceTexture,
	uint32 NumSamples = 1);

UE_DEPRECATED(5.1, "RHICreateTargetableShaderResource2DArray is deprecated. RHICreateTexture should be used instead.")
RHI_API void RHICreateTargetableShaderResource2DArray(
	uint32 SizeX,
	uint32 SizeY,
	uint32 SizeZ,
	uint8 Format,
	uint32 NumMips,
	ETextureCreateFlags Flags,
	ETextureCreateFlags TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	bool bForceSharedTargetAndShaderResource,
	const FRHIResourceCreateInfo& CreateInfo,
	FTextureRHIRef& OutTargetableTexture,
	FTextureRHIRef& OutShaderResourceTexture,
	uint32 NumSamples = 1);

UE_DEPRECATED(5.1, "RHICreateTargetableShaderResource2DArray is deprecated. RHICreateTexture should be used instead.")
RHI_API void RHICreateTargetableShaderResource2DArray(
	uint32 SizeX,
	uint32 SizeY,
	uint32 SizeZ,
	uint8 Format,
	uint32 NumMips,
	ETextureCreateFlags Flags,
	ETextureCreateFlags TargetableTextureFlags,
	const FRHIResourceCreateInfo& CreateInfo,
	FTextureRHIRef& OutTargetableTexture,
	FTextureRHIRef& OutShaderResourceTexture,
	uint32 NumSamples = 1);

/**
 * Creates 1 or 2 textures with the same dimensions/format.
 * If the RHI supports textures that can be used as both shader resources and render targets,
 * and bForceSeparateTargetAndShaderResource=false, then a single texture is created.
 * Otherwise two textures are created, one of them usable as a shader resource and resolve target, and one of them usable as a render target.
 * Two texture references are always returned, but they may reference the same texture.
 * If two different textures are returned, the render-target texture must be manually copied to the shader-resource texture.
 */
UE_DEPRECATED(5.1, "RHICreateTargetableShaderResourceCube is deprecated. RHICreateTexture should be used instead.")
RHI_API void RHICreateTargetableShaderResourceCube(
	uint32 LinearSize,
	uint8 Format,
	uint32 NumMips,
	ETextureCreateFlags Flags,
	ETextureCreateFlags TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	const FRHIResourceCreateInfo& CreateInfo,
	FTextureRHIRef& OutTargetableTexture,
	FTextureRHIRef& OutShaderResourceTexture);

/**
 * Creates 1 or 2 textures with the same dimensions/format.
 * If the RHI supports textures that can be used as both shader resources and render targets,
 * and bForceSeparateTargetAndShaderResource=false, then a single texture is created.
 * Otherwise two textures are created, one of them usable as a shader resource and resolve target, and one of them usable as a render target.
 * Two texture references are always returned, but they may reference the same texture.
 * If two different textures are returned, the render-target texture must be manually copied to the shader-resource texture.
 */
UE_DEPRECATED(5.1, "RHICreateTargetableShaderResourceCubeArray is deprecated. RHICreateTexture should be used instead.")
RHI_API void RHICreateTargetableShaderResourceCubeArray(
	uint32 LinearSize,
	uint32 ArraySize,
	uint8 Format,
	uint32 NumMips,
	ETextureCreateFlags Flags,
	ETextureCreateFlags TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	const FRHIResourceCreateInfo& CreateInfo,
	FTextureRHIRef& OutTargetableTexture,
	FTextureRHIRef& OutShaderResourceTexture);

/**
 * Creates 1 or 2 textures with the same dimensions/format.
 * If the RHI supports textures that can be used as both shader resources and render targets,
 * and bForceSeparateTargetAndShaderResource=false, then a single texture is created.
 * Otherwise two textures are created, one of them usable as a shader resource and resolve target, and one of them usable as a render target.
 * Two texture references are always returned, but they may reference the same texture.
 * If two different textures are returned, the render-target texture must be manually copied to the shader-resource texture.
 */
UE_DEPRECATED(5.1, "RHICreateTargetableShaderResource3D is deprecated. RHICreateTexture should be used instead.")
RHI_API void RHICreateTargetableShaderResource3D(
	uint32 SizeX,
	uint32 SizeY,
	uint32 SizeZ,
	uint8 Format,
	uint32 NumMips,
	ETextureCreateFlags Flags,
	ETextureCreateFlags TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	const FRHIResourceCreateInfo& CreateInfo,
	FTextureRHIRef& OutTargetableTexture,
	FTextureRHIRef& OutShaderResourceTexture);

/** Performs a clear render pass on an RHI texture. The texture is expected to be in the RTV state. */
inline void ClearRenderTarget(FRHICommandList& RHICmdList, FRHITexture* Texture, uint32 MipIndex = 0, uint32 ArraySlice = 0)
{
	check(Texture);
	const FIntPoint Extent = Texture->GetSizeXY();
	FRHIRenderPassInfo Info(Texture, ERenderTargetActions::Clear_Store);
	Info.ColorRenderTargets[0].MipIndex = (uint8)MipIndex;
	Info.ColorRenderTargets[0].ArraySlice = (int32)ArraySlice;
	RHICmdList.BeginRenderPass(Info, TEXT("ClearRenderTarget"));
	RHICmdList.EndRenderPass();
}

inline void TransitionAndCopyTexture(FRHICommandList& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FRHICopyTextureInfo& Info)
{
	check(SrcTexture && DstTexture);
	check(SrcTexture->GetNumSamples() == DstTexture->GetNumSamples());

	if (SrcTexture == DstTexture)
	{
		RHICmdList.Transition({
			FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask)
		});
		return;
	}

	RHICmdList.Transition({
		FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc),
		FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest)
	});

	RHICmdList.CopyTexture(SrcTexture, DstTexture, Info);

	RHICmdList.Transition({
		FRHITransitionInfo(SrcTexture, ERHIAccess::CopySrc,  ERHIAccess::SRVMask),
		FRHITransitionInfo(DstTexture, ERHIAccess::CopyDest, ERHIAccess::SRVMask)
	});
}

/**
 * Computes the vertex count for a given number of primitives of the specified type.
 * @param NumPrimitives The number of primitives.
 * @param PrimitiveType The type of primitives.
 * @returns The number of vertices.
 */
inline uint32 GetVertexCountForPrimitiveCount(uint32 NumPrimitives, uint32 PrimitiveType)
{
	static_assert(PT_Num == 38, "This function needs to be updated");
	uint32 Factor = (PrimitiveType == PT_TriangleList)? 3 : (PrimitiveType == PT_LineList)? 2 : (PrimitiveType == PT_RectList)? 3 : (PrimitiveType >= PT_1_ControlPointPatchList)? (PrimitiveType - PT_1_ControlPointPatchList + 1) : 1;
	uint32 Offset = (PrimitiveType == PT_TriangleStrip)? 2 : 0;

	return NumPrimitives * Factor + Offset;

}

inline uint32 ComputeAnisotropyRT(int32 InitializerMaxAnisotropy)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaxAnisotropy"));
	int32 CVarValue = CVar->GetValueOnAnyThread(); // this is sometimes called from main thread during initialization of static RHI states

	return FMath::Clamp(InitializerMaxAnisotropy > 0 ? InitializerMaxAnisotropy : CVarValue, 1, 16);
}

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
#define ENABLE_TRANSITION_DUMP 0
#else
#define ENABLE_TRANSITION_DUMP 1
#endif

class RHI_API FDumpTransitionsHelper
{
public:
	static void DumpResourceTransition(const FName& ResourceName, const ERHIAccess TransitionType);
	
private:
	static void DumpTransitionForResourceHandler();

	static TAutoConsoleVariable<FString> CVarDumpTransitionsForResource;
	static FAutoConsoleVariableSink CVarDumpTransitionsForResourceSink;
	static FName DumpTransitionForResource;
};

#if ENABLE_TRANSITION_DUMP
#define DUMP_TRANSITION(ResourceName, TransitionType) FDumpTransitionsHelper::DumpResourceTransition(ResourceName, TransitionType);
#else
#define DUMP_TRANSITION(ResourceName, TransitionType)
#endif

extern RHI_API void SetDepthBoundsTest(FRHICommandList& RHICmdList, float WorldSpaceDepthNear, float WorldSpaceDepthFar, const FMatrix& ProjectionMatrix);

/** Returns the value of the rhi.SyncInterval CVar. */
extern RHI_API uint32 RHIGetSyncInterval();

/** Returns the value of the rhi.SyncSlackMS CVar or length of a full frame interval if the frame offset system is disabled. */
extern RHI_API float RHIGetSyncSlackMS();

/** Returns the top and bottom vsync present thresholds (the values of rhi.PresentThreshold.Top and rhi.PresentThreshold.Bottom) */
extern RHI_API void RHIGetPresentThresholds(float& OutTopPercent, float& OutBottomPercent);

/** Returns the value of the rhi.SyncAllowVariable CVar. */
extern RHI_API bool RHIGetSyncAllowVariable();

/** Signals the completion of the specified task graph event when the given frame has flipped. */
extern RHI_API void RHICompleteGraphEventOnFlip(uint64 PresentIndex, FGraphEventRef Event);

/** Sets the FrameIndex and InputTime for the current frame. */
extern RHI_API void RHISetFrameDebugInfo(uint64 PresentIndex, uint64 FrameIndex, uint64 InputTime);

extern RHI_API void RHIInitializeFlipTracking();
extern RHI_API void RHIShutdownFlipTracking();

/** Sets the FrameIndex and InputTime for the current frame. */
extern RHI_API float RHIGetFrameTime();

extern RHI_API void RHICalculateFrameTime();

struct FRHILockTracker
{
	struct FLockParams
	{
		void* RHIBuffer;
		void* Buffer;
		uint32 BufferSize;
		uint32 Offset;
		EResourceLockMode LockMode;
		bool bDirectLock; //did we call the normal flushing/updating lock?
		bool bCreateLock; //did we lock to immediately initialize a newly created buffer?
		
		FORCEINLINE_DEBUGGABLE FLockParams(void* InRHIBuffer, void* InBuffer, uint32 InOffset, uint32 InBufferSize, EResourceLockMode InLockMode, bool bInbDirectLock, bool bInCreateLock)
		: RHIBuffer(InRHIBuffer)
		, Buffer(InBuffer)
		, BufferSize(InBufferSize)
		, Offset(InOffset)
		, LockMode(InLockMode)
		, bDirectLock(bInbDirectLock)
		, bCreateLock(bInCreateLock)
		{
		}
	};
	
	struct FUnlockFenceParams
	{
		FUnlockFenceParams(void* InRHIBuffer, FGraphEventRef InUnlockEvent)
		: RHIBuffer(InRHIBuffer)
		, UnlockEvent(InUnlockEvent)
		{
			
		}
		void* RHIBuffer;
		FGraphEventRef UnlockEvent;
	};
	
	TArray<FLockParams, TInlineAllocator<16> > OutstandingLocks;
	uint32 TotalMemoryOutstanding;
	TArray<FUnlockFenceParams, TInlineAllocator<16> > OutstandingUnlocks;
	
	FRHILockTracker()
	{
		TotalMemoryOutstanding = 0;
	}
	
	FORCEINLINE_DEBUGGABLE void Lock(void* RHIBuffer, void* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode, bool bInDirectBufferWrite = false, bool bInCreateLock = false)
	{
#if DO_CHECK
		for (auto& Parms : OutstandingLocks)
		{
			check((Parms.RHIBuffer != RHIBuffer) || (Parms.bDirectLock && bInDirectBufferWrite) || Parms.Offset != Offset);
		}
#endif
		OutstandingLocks.Add(FLockParams(RHIBuffer, Buffer, Offset, SizeRHI, LockMode, bInDirectBufferWrite, bInCreateLock));
		TotalMemoryOutstanding += SizeRHI;
	}
	FORCEINLINE_DEBUGGABLE FLockParams Unlock(void* RHIBuffer, uint32 Offset=0)
	{
		for (int32 Index = 0; Index < OutstandingLocks.Num(); Index++)
		{
			if (OutstandingLocks[Index].RHIBuffer == RHIBuffer && OutstandingLocks[Index].Offset == Offset)
			{
				FLockParams Result = OutstandingLocks[Index];
				OutstandingLocks.RemoveAtSwap(Index, 1, false);
				return Result;
			}
		}
		UE_LOG(LogRHI, Fatal, TEXT("Mismatched RHI buffer locks."));
		return FLockParams(nullptr, nullptr, 0, 0, RLM_WriteOnly, false, false);
	}
	
	template<class TIndexOrVertexBufferPointer>
	FORCEINLINE_DEBUGGABLE void AddUnlockFence(TIndexOrVertexBufferPointer* Buffer, FRHICommandListImmediate& RHICmdList, const FLockParams& LockParms)
	{
		if (LockParms.LockMode != RLM_WriteOnly || !(Buffer->GetUsage() & BUF_Volatile))
		{
			OutstandingUnlocks.Emplace(Buffer, RHICmdList.RHIThreadFence(true));
		}
	}
	
	FORCEINLINE_DEBUGGABLE void WaitForUnlock(void* RHIBuffer)
	{
		for (int32 Index = 0; Index < OutstandingUnlocks.Num(); Index++)
		{
			if (OutstandingUnlocks[Index].RHIBuffer == RHIBuffer)
			{
				FRHICommandListExecutor::WaitOnRHIThreadFence(OutstandingUnlocks[Index].UnlockEvent);
				OutstandingUnlocks.RemoveAtSwap(Index, 1, false);
				return;
			}
		}
	}
	
	FORCEINLINE_DEBUGGABLE void FlushCompleteUnlocks()
	{
		uint32 Count = OutstandingUnlocks.Num();
		for (uint32 Index = 0; Index < Count; Index++)
		{
			if (OutstandingUnlocks[Index].UnlockEvent->IsComplete())
			{
				OutstandingUnlocks.RemoveAt(Index, 1);
				--Count;
				--Index;
			}
		}
	}
};

extern RHI_API FRHILockTracker GRHILockTracker;
