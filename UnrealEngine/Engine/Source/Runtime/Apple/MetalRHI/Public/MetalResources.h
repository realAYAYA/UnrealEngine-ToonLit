// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalResources.h: Metal resource RHI definitions..
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "MetalShaderResources.h"
#include "ShaderCodeArchive.h"

THIRD_PARTY_INCLUDES_START
#include "mtlpp.hpp"
THIRD_PARTY_INCLUDES_END

class FMetalContext;
@class FMetalShaderPipeline;

extern NSString* DecodeMetalSourceCode(uint32 CodeSize, TArray<uint8> const& CompressedSource);

struct FMetalRenderPipelineHash
{
	friend uint32 GetTypeHash(FMetalRenderPipelineHash const& Hash)
	{
		return HashCombine(GetTypeHash(Hash.RasterBits), GetTypeHash(Hash.TargetBits));
	}
	
	friend bool operator==(FMetalRenderPipelineHash const& Left, FMetalRenderPipelineHash const& Right)
	{
		return Left.RasterBits == Right.RasterBits && Left.TargetBits == Right.TargetBits;
	}
	
	uint64 RasterBits;
	uint64 TargetBits;
};

class FMetalSubBufferHeap;
class FMetalSubBufferLinear;
class FMetalSubBufferMagazine;

class FMetalBuffer : public mtlpp::Buffer
{
public:
	FMetalBuffer(ns::Ownership retain = ns::Ownership::Retain) : mtlpp::Buffer(retain), Heap(nullptr), Linear(nullptr), Magazine(nullptr), bPooled(false) { }
	FMetalBuffer(ns::Protocol<id<MTLBuffer>>::type handle, ns::Ownership retain = ns::Ownership::Retain);
	
	FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferHeap* heap);
	FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferLinear* heap);
	FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferMagazine* magazine);
	FMetalBuffer(mtlpp::Buffer&& rhs, bool bInPooled);
	
	FMetalBuffer(const FMetalBuffer& rhs);
	FMetalBuffer(FMetalBuffer&& rhs);
	virtual ~FMetalBuffer();
	
	FMetalBuffer& operator=(const FMetalBuffer& rhs);
	FMetalBuffer& operator=(FMetalBuffer&& rhs);
	
	inline bool operator==(FMetalBuffer const& rhs) const
	{
		return mtlpp::Buffer::operator==(rhs);
	}
	
	inline bool IsPooled() const { return bPooled; }
	inline bool IsSingleUse() const { return bSingleUse; }
	inline void MarkSingleUse() { bSingleUse = true; }
    void SetOwner(class FMetalRHIBuffer* Owner, bool bIsSwap);
	void Release();
	
	friend uint32 GetTypeHash(FMetalBuffer const& Hash)
	{
		return HashCombine(GetTypeHash(Hash.GetPtr()), GetTypeHash((uint64)Hash.GetOffset()));
	}
	
private:
	FMetalSubBufferHeap* Heap;
	FMetalSubBufferLinear* Linear;
	FMetalSubBufferMagazine* Magazine;
	bool bPooled;
	bool bSingleUse;
};

class FMetalTexture : public mtlpp::Texture
{
public:
	FMetalTexture(ns::Ownership retain = ns::Ownership::Retain)
		: mtlpp::Texture(retain)
	{}

	FMetalTexture(ns::Protocol<id<MTLTexture>>::type handle, ns::Ownership retain = ns::Ownership::Retain)
		: mtlpp::Texture(handle, nullptr, retain)
	{}
	
	FMetalTexture(mtlpp::Texture&& rhs)
		: mtlpp::Texture((mtlpp::Texture&&)rhs)
	{}
	
	FMetalTexture(const FMetalTexture& rhs)
		: mtlpp::Texture(rhs)
	{}
	
	FMetalTexture(FMetalTexture&& rhs)
		: mtlpp::Texture((mtlpp::Texture&&)rhs)
	{}
	
	FMetalTexture& operator=(const FMetalTexture& rhs)
	{
		if (this != &rhs)
		{
			mtlpp::Texture::operator=(rhs);
		}
		return *this;
	}
	
	FMetalTexture& operator=(FMetalTexture&& rhs)
	{
		mtlpp::Texture::operator=((mtlpp::Texture&&)rhs);
		return *this;
	}
	
	inline bool operator==(FMetalTexture const& rhs) const
	{
		return mtlpp::Texture::operator==(rhs);
	}
	
	friend uint32 GetTypeHash(FMetalTexture const& Hash)
	{
		return GetTypeHash(Hash.GetPtr());
	}
};

struct FMetalTextureCreateDesc : public FRHITextureCreateDesc
{
	FMetalTextureCreateDesc(FRHITextureCreateDesc const& CreateDesc);

	mtlpp::TextureDescriptor Desc;
	mtlpp::PixelFormat MTLFormat;
	bool bIsRenderTarget = false;
	uint8 FormatKey = 0;
};

// Metal RHI texture resource
class METALRHI_API FMetalSurface : public FRHITexture
{
public:

	/** 
	 * Constructor that will create Texture and Color/DepthBuffers as needed
	 */
	FMetalSurface(FMetalTextureCreateDesc const& CreateDesc);
	
	/**
	 * Destructor
	 */
	virtual ~FMetalSurface();

	/** Prepare for texture-view support - need only call this once on the source texture which is to be viewed. */
	void PrepareTextureView();
	
	/** @returns A newly allocated buffer object large enough for the surface within the texture specified. */
	id <MTLBuffer> AllocSurface(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool SingleLayer = false);

	/** Apply the data in Buffer to the surface specified.
	 * Will also handle destroying SourceBuffer appropriately.
	 */
	void UpdateSurfaceAndDestroySourceBuffer(id <MTLBuffer> SourceBuffer, uint32 MipIndex, uint32 ArrayIndex);
	
	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool SingleLayer = false);
	
	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex, bool bTryAsync);
	
	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* AsyncLock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool bNeedsDefaultRHIFlush);
	
	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void AsyncUnlock(id <MTLBuffer> SourceData, uint32 MipIndex, uint32 ArrayIndex);

	/**
	 * Returns how much memory a single mip uses, and optionally returns the stride
	 */
	uint32 GetMipSize(uint32 MipIndex, uint32* Stride, bool bSingleLayer);

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize();

	/** Returns the number of faces for the texture */
	uint32 GetNumFaces();
	
	/** Gets the drawable texture if this is a back-buffer surface. */
	FMetalTexture GetDrawableTexture();
	ns::AutoReleased<FMetalTexture> GetCurrentTexture();

	FMetalTexture Reallocate(FMetalTexture Texture, mtlpp::TextureUsage UsageModifier);
	void MakeAliasable(void);
	
	int16 volatile Written;
	uint8 const FormatKey;

	//texture used for store actions and binding to shader params
	FMetalTexture Texture;
	//if surface is MSAA, texture used to bind for RT
	FMetalTexture MSAATexture;

	//texture used for a resolve target.  Same as texture on iOS.  
	//Dummy target on Mac where RHISupportsSeparateMSAAAndResolveTextures is true.	In this case we don't always want a resolve texture but we
	//have to have one until renderpasses are implemented at a high level.
	// Mac / RHISupportsSeparateMSAAAndResolveTextures == true
	// iOS A9+ where depth resolve is available
	// iOS < A9 where depth resolve is unavailable.
	FMetalTexture MSAAResolveTexture;

	// how much memory is allocated for this texture
	uint64 TotalTextureSize;
	
	// For back-buffers, the owning viewport.
	class FMetalViewport* Viewport;

	virtual void* GetTextureBaseRHI() override final
	{
		return this;
	}

	virtual void* GetNativeResource() const override final
	{
		return Texture;
	}
	
private:
	// The movie playback IOSurface/CVTexture wrapper to avoid page-off
	CFTypeRef ImageSurfaceRef;

	// Count of outstanding async. texture uploads
	static volatile int64 ActiveUploads;
};

@interface FMetalBufferData : FApplePlatformObject<NSObject>
{
@public
	uint8* Data;
	uint32 Len;	
}
-(instancetype)initWithSize:(uint32)Size;
-(instancetype)initWithBytes:(void const*)Data length:(uint32)Size;
@end

enum class EMetalBufferUsage
{
	None = 0,
	GPUOnly = 1 << 0,
	LinearTex = 1 << 1,
};
ENUM_CLASS_FLAGS(EMetalBufferUsage);

class FMetalLinearTextureDescriptor
{
public:
	FMetalLinearTextureDescriptor() = default;

	FMetalLinearTextureDescriptor(uint32 InStartOffsetBytes, uint32 InNumElements, uint32 InBytesPerElement)
		: StartOffsetBytes(InStartOffsetBytes)
		, NumElements     (InNumElements)
		, BytesPerElement (InBytesPerElement)
	{}

	friend uint32 GetTypeHash(FMetalLinearTextureDescriptor const& Key)
	{
		uint32 Hash = GetTypeHash((uint64)Key.StartOffsetBytes);
		Hash = HashCombine(Hash, GetTypeHash((uint64)Key.NumElements));
		Hash = HashCombine(Hash, GetTypeHash((uint64)Key.BytesPerElement));
		return Hash;
	}

	bool operator==(FMetalLinearTextureDescriptor const& Other) const
	{
		return    StartOffsetBytes == Other.StartOffsetBytes
		       && NumElements      == Other.NumElements
		       && BytesPerElement  == Other.BytesPerElement;
	}

	uint32 StartOffsetBytes = 0;
	uint32 NumElements      = UINT_MAX;
	uint32 BytesPerElement  = 0;
};

class FMetalRHIBuffer
{
public:
	// Matches other RHIs
	static constexpr const uint32 MetalMaxNumBufferedFrames = 4;
	
	using LinearTextureMapKey = TTuple<EPixelFormat, FMetalLinearTextureDescriptor>;
	using LinearTextureMap = TMap<LinearTextureMapKey, FMetalTexture>;
	
	struct FMetalBufferAndViews
	{
		FMetalBuffer Buffer;
		LinearTextureMap Views;
	};
	
	FMetalRHIBuffer(uint32 InSize, EBufferUsageFlags InUsage, EMetalBufferUsage InMetalUsage, ERHIResourceType InType);
	virtual ~FMetalRHIBuffer();
	
	/**
	 * Initialize the buffer contents from the render-thread.
	 */
	void Init(class FRHICommandListBase& RHICmdList, uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo, FRHIResource* Resource);
	
	/**
	 * Get a linear texture for given format.
	 */
	void CreateLinearTexture(EPixelFormat InFormat, FRHIResource* InParent, const FMetalLinearTextureDescriptor* InLinearTextureDescriptor = nullptr);
	
	/**
	 * Get a linear texture for given format.
	 */
	ns::AutoReleased<FMetalTexture> GetLinearTexture(EPixelFormat InFormat, const FMetalLinearTextureDescriptor* InLinearTextureDescriptor = nullptr);
	
	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(bool bIsOnRHIThread, EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);
	
	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	void Swap(FMetalRHIBuffer& Other);
	
	const FMetalBufferAndViews& GetCurrentBacking()
	{
		check(NumberOfBuffers > 0);
		return BufferPool[CurrentIndex];
	}
	
	const FMetalBuffer& GetCurrentBuffer()
	{
		return BufferPool[CurrentIndex].Buffer;
	}
	
	FMetalBuffer GetCurrentBufferOrNil()
	{
		if(NumberOfBuffers > 0)
		{
			return GetCurrentBuffer();
		}
		return nil;
	}
	
	EMetalBufferUsage GetMetalUsage() const
	{
		return MetalUsage;
	}
	
	void AdvanceBackingIndex()
	{
		CurrentIndex = (CurrentIndex + 1) % NumberOfBuffers;
	}
	
	/**
	 * Whether to allocate the resource from private memory.
	 */
	bool UsePrivateMemory() const;
	
	// A temporary shared/CPU accessible buffer for upload/download
	FMetalBuffer TransferBuffer;
	
	TArray<FMetalBufferAndViews> BufferPool;
	
	/** Buffer for small buffers < 4Kb to avoid heap fragmentation. */
	FMetalBufferData* Data;
	
	// Frame we last locked (for debugging, mainly)
	uint32 LastLockFrame;
	
	// The active buffer.
	uint32 CurrentIndex		: 8;
	// How many buffers are actually allocated
	uint32 NumberOfBuffers	: 8;
	// Current lock mode. RLM_Num indicates this buffer is not locked.
	uint32 CurrentLockMode	: 16;
	
	// offset into the buffer (for lock usage)
	uint32 LockOffset;
	
	// Sizeof outstanding lock.
	uint32 LockSize;
	
	// Initial buffer size.
	uint32 Size;
	
	// Buffer usage.
	EBufferUsageFlags Usage;
	
	// Metal buffer usage.
	EMetalBufferUsage MetalUsage;
	
	// Storage mode
	mtlpp::StorageMode Mode;
	
	// Resource type
	ERHIResourceType Type;
	
	static_assert((1 << 16) > RLM_Num, "Lock mode does not fit in bitfield");
	static_assert((1 << 8) > MetalMaxNumBufferedFrames, "Buffer count does not fit in bitfield");
	
private:
	FMetalBufferAndViews& GetCurrentBackingInternal()
	{
		return BufferPool[CurrentIndex];
	}
	
	FMetalBuffer& GetCurrentBufferInternal()
	{
		return BufferPool[CurrentIndex].Buffer;
	}
	
	/**
	 * Allocate the CPU accessible buffer for data transfer.
	 */
	void AllocTransferBuffer(bool bOnRHIThread, uint32 InSize, EResourceLockMode LockMode);
	
	/**
	 * Allocate a linear texture for given format.
	 */
	void AllocLinearTextures(const LinearTextureMapKey& InLinearTextureMapKey);
};

class FMetalResourceMultiBuffer : public FRHIBuffer, public FMetalRHIBuffer
{
public:
	FMetalResourceMultiBuffer(uint32 InSize, EBufferUsageFlags InUsage, EMetalBufferUsage InMetalUsage, uint32 InStride, ERHIResourceType ResourceType);
	virtual ~FMetalResourceMultiBuffer();

	void Swap(FMetalResourceMultiBuffer& Other);

	// 16- or 32-bit; used for index buffers only.
	mtlpp::IndexType IndexType;
};

typedef FMetalResourceMultiBuffer FMetalIndexBuffer;
typedef FMetalResourceMultiBuffer FMetalVertexBuffer;
typedef FMetalResourceMultiBuffer FMetalStructuredBuffer;

class FMetalResourceViewBase
{
protected:
	// Constructor for buffers
	FMetalResourceViewBase(
		  FRHIBuffer* InBuffer
		, uint32 InStartOffsetBytes
		, uint32 InNumElements
		, EPixelFormat InFormat
	);

	// Constructor for textures
	FMetalResourceViewBase(
		  FRHITexture* InTexture
		, EPixelFormat InFormat
		, uint8 InMipLevel
		, uint8 InNumMipLevels
		, ERHITextureSRVOverrideSRGBType InSRGBOverride
		, uint32 InFirstArraySlice
		, uint32 InNumArraySlices
		, bool bInUAV
	);

public:
	~FMetalResourceViewBase();

	inline FMetalResourceMultiBuffer* GetSourceBuffer () const { check(!bTexture); return SourceBuffer; }

	inline FMetalSurface*             GetSourceTexture() const { check(bTexture); return SourceTexture; }
	inline FMetalTexture const&       GetTextureView  () const { check(bTexture); return TextureView;   }

private:
	// Needed for RHIUpdateShaderResourceView
	friend class FMetalDynamicRHI;

	union
	{
		FMetalResourceMultiBuffer* SourceBuffer;
		FMetalSurface* SourceTexture;
	};

	TUniquePtr<FMetalLinearTextureDescriptor> LinearTextureDesc = nullptr;
	FMetalTexture TextureView = nullptr;

public:
	uint8 const bTexture : 1;
	uint8       bSRGBForceDisable : 1;
	uint8       MipLevel : 4;
	uint8       Reserved : 2;
	uint8       NumMips;
	uint8       Format;
	uint8       Stride;
	uint32      Offset;

	ns::AutoReleased<FMetalTexture> GetLinearTexture();
};

class FMetalShaderResourceView final : public FRHIShaderResourceView, public FMetalResourceViewBase
{
public:
	explicit FMetalShaderResourceView(const FShaderResourceViewInitializer& Initializer)
		: FRHIShaderResourceView(Initializer.AsBufferSRV().Buffer)
		, FMetalResourceViewBase(
			  Initializer.AsBufferSRV().Buffer
			, Initializer.AsBufferSRV().StartOffsetBytes
			, Initializer.AsBufferSRV().NumElements
			, Initializer.AsBufferSRV().Format
		)
	{}

	explicit FMetalShaderResourceView(FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo)
		: FRHIShaderResourceView(Texture)
		, FMetalResourceViewBase(
			  Texture
			, CreateInfo.Format
			, CreateInfo.MipLevel
			, CreateInfo.NumMipLevels
			, CreateInfo.SRGBOverride
			, CreateInfo.FirstArraySlice
			, CreateInfo.NumArraySlices
			, false // bInUAV
		)
	{}

	virtual ~FMetalShaderResourceView()
	{}
};

class FMetalUnorderedAccessView final : public FRHIUnorderedAccessView, public FMetalResourceViewBase
{
public:
	explicit FMetalUnorderedAccessView(FRHIBuffer* Buffer, EPixelFormat Format)
		: FRHIUnorderedAccessView(Buffer)
		, FMetalResourceViewBase(Buffer, 0, UINT_MAX, Format)
	{}

	explicit FMetalUnorderedAccessView(FRHIBuffer* Buffer, bool bUseUAVCounter, bool bAppendBuffer)
		: FRHIUnorderedAccessView(Buffer)
		, FMetalResourceViewBase(Buffer, 0, UINT_MAX, PF_Unknown)
	{
		checkf(!bUseUAVCounter, TEXT("UAV counters not implemented."));
		checkf(!bAppendBuffer, TEXT("UAV append buffers not implemented."));
	}

	explicit FMetalUnorderedAccessView(FRHITexture* Texture, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices)
		: FRHIUnorderedAccessView(Texture)
		, FMetalResourceViewBase(
			  Texture
			, PF_Unknown
			, MipLevel
			, 1 // NumMipLevels
			, ERHITextureSRVOverrideSRGBType::SRGBO_ForceDisable
			, FirstArraySlice
			, NumArraySlices
			, true // bInUAV
		)
	{}

	virtual ~FMetalUnorderedAccessView()
	{}
};

class FMetalGPUFence final : public FRHIGPUFence
{
public:
	FMetalGPUFence(FName InName)
		: FRHIGPUFence(InName)
	{
	}

	~FMetalGPUFence()
	{
	}

	virtual void Clear() override final;

	void WriteInternal(mtlpp::CommandBuffer& CmdBuffer);

	virtual bool Poll() const override final;

private:
	mtlpp::CommandBufferFence Fence;
};

class FMetalShaderLibrary;
class FMetalGraphicsPipelineState;
class FMetalComputePipelineState;
class FMetalVertexDeclaration;
class FMetalVertexShader;
class FMetalGeometryShader;
class FMetalPixelShader;
class FMetalComputeShader;
class FMetalRHIStagingBuffer;
class FMetalRHIRenderQuery;
class FMetalSuballocatedUniformBuffer;

template<class T>
struct TMetalResourceTraits
{
};
template<>
struct TMetalResourceTraits<FRHIShaderLibrary>
{
	typedef FMetalShaderLibrary TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexDeclaration>
{
	typedef FMetalVertexDeclaration TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexShader>
{
	typedef FMetalVertexShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIGeometryShader>
{
	typedef FMetalGeometryShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIPixelShader>
{
	typedef FMetalPixelShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputeShader>
{
	typedef FMetalComputeShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRenderQuery>
{
	typedef FMetalRHIRenderQuery TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIUniformBuffer>
{
	typedef FMetalSuballocatedUniformBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIBuffer>
{
	typedef FMetalResourceMultiBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIShaderResourceView>
{
	typedef FMetalShaderResourceView TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIUnorderedAccessView>
{
	typedef FMetalUnorderedAccessView TConcreteType;
};

template<>
struct TMetalResourceTraits<FRHISamplerState>
{
	typedef FMetalSamplerState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRasterizerState>
{
	typedef FMetalRasterizerState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIDepthStencilState>
{
	typedef FMetalDepthStencilState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIBlendState>
{
	typedef FMetalBlendState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIGraphicsPipelineState>
{
	typedef FMetalGraphicsPipelineState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputePipelineState>
{
	typedef FMetalComputePipelineState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIGPUFence>
{
	typedef FMetalGPUFence TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIStagingBuffer>
{
	typedef FMetalRHIStagingBuffer TConcreteType;
};
