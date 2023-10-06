// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalResources.h: Metal resource RHI definitions..
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "MetalShaderResources.h"
#include "ShaderCodeArchive.h"

#define UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER 1

class FMetalRHICommandContext;

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

class FMetalResourceViewBase;
class FMetalShaderResourceView;
class FMetalUnorderedAccessView;

class FMetalViewableResource
{
public:
	~FMetalViewableResource()
	{
		checkf(!HasLinkedViews(), TEXT("All linked views must have been removed before the underlying resource can be deleted."));
	}

	bool HasLinkedViews() const
	{
		return LinkedViews != nullptr;
	}

	void UpdateLinkedViews();

private:
	friend FMetalShaderResourceView;
	friend FMetalUnorderedAccessView;
	FMetalResourceViewBase* LinkedViews = nullptr;
};

// Metal RHI texture resource
class METALRHI_API FMetalSurface : public FRHITexture, public FMetalViewableResource
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
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool SingleLayer = false, uint64* OutLockedByteCount = nullptr);
	
	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex, bool bTryAsync);
	
	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* AsyncLock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool bNeedsDefaultRHIFlush, uint64* OutLockedByteCount = nullptr);
	
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

class FMetalRHIBuffer final : public FRHIBuffer, public FMetalViewableResource
{
public:
	// Matches other RHIs
	static constexpr const uint32 MetalMaxNumBufferedFrames = 4;
	
	FMetalRHIBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& InBufferDesc, FRHIResourceCreateInfo& CreateInfo);
	virtual ~FMetalRHIBuffer();
	
	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(bool bIsOnRHIThread, EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);
	
	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	FMetalBuffer& GetCurrentBuffer()
	{
		return BufferPool[CurrentIndex];
	}
	
	FMetalBuffer GetCurrentBufferOrNil()
	{
		if (NumberOfBuffers > 0)
		{
			return GetCurrentBuffer();
		}
		return nil;
	}
	
	void AdvanceBackingIndex()
	{
		CurrentIndex = (CurrentIndex + 1) % NumberOfBuffers;
	}
	
#if METAL_RHI_RAYTRACING
	bool IsAccelerationStructure() const
	{
		return EnumHasAnyFlags(Usage, BUF_AccelerationStructure);
	}
	mtlpp::AccelerationStructure AccelerationStructureHandle;
#endif // METAL_RHI_RAYTRACING

	/**
	 * Whether to allocate the resource from private memory.
	 */
	bool UsePrivateMemory() const;
	
    void TakeOwnership(FMetalRHIBuffer& Other);
    void ReleaseOwnership();
    
	// A temporary shared/CPU accessible buffer for upload/download
	FMetalBuffer TransferBuffer;
	
	TArray<FMetalBuffer> BufferPool;
	
	/** Buffer for small buffers < 4Kb to avoid heap fragmentation. */
	FMetalBufferData* Data = nullptr;
	
	// The active buffer.
	uint8 CurrentIndex = 0;
	// How many buffers are actually allocated
	uint8 NumberOfBuffers = 0;
	// Current lock mode. RLM_Num indicates this buffer is not locked.
	uint16 CurrentLockMode = RLM_Num;
	
	// offset into the buffer (for lock usage)
	uint32 LockOffset = 0;
	
	// Sizeof outstanding lock.
	uint32 LockSize = 0;
	
	// Initial buffer size.
	uint32 Size;
	
	// Storage mode
	mtlpp::StorageMode Mode;
	
	// 16- or 32-bit; used for index buffers only.
	mtlpp::IndexType GetIndexType() const
	{
		return GetStride() == 2
			? mtlpp::IndexType::UInt16
			: mtlpp::IndexType::UInt32;
	}

	static_assert((1 << 16) > RLM_Num, "Lock mode does not fit in bitfield");
	static_assert((1 << 8) > MetalMaxNumBufferedFrames, "Buffer count does not fit in bitfield");
	
private:
	// Allocate the CPU accessible buffer for data transfer.
	void AllocTransferBuffer(bool bOnRHIThread, uint32 InSize, EResourceLockMode LockMode);
};

class FMetalResourceViewBase : public TIntrusiveLinkedList<FMetalResourceViewBase>
{
public:
	struct FBufferView
	{
		FMetalBuffer& Buffer;
		uint32 Offset;
		uint32 Size;

		FBufferView(FMetalBuffer& Buffer, uint32 Offset, uint32 Size)
			: Buffer(Buffer)
			, Offset(Offset)
			, Size(Size)
		{}
	};
    
    struct FTextureBufferBacked
    {
        FMetalTexture Texture;
        FMetalBuffer Buffer;
        uint32 Offset;
        uint32 Size;
        EPixelFormat Format;

        FTextureBufferBacked(FMetalTexture& Texture, FMetalBuffer& Buffer, uint32 Offset, uint32 Size, EPixelFormat Format)
            :
              Texture(Texture)
            , Buffer(Buffer)
            , Offset(Offset)
            , Size(Size)
            , Format(Format)
        {}
    };

	typedef TVariant<FEmptyVariantState
		, FMetalTexture
		, FBufferView
        , FTextureBufferBacked
#if METAL_RHI_RAYTRACING
		, mtlpp::AccelerationStructure
#endif
	> TStorage;

	enum class EMetalType
	{
		Null                  = TStorage::IndexOfType<FEmptyVariantState>(),
		TextureView           = TStorage::IndexOfType<FMetalTexture>(),
		BufferView            = TStorage::IndexOfType<FBufferView>(),
        TextureBufferBacked   = TStorage::IndexOfType<FTextureBufferBacked>(),
#if METAL_RHI_RAYTRACING
		AccelerationStructure = TStorage::IndexOfType<mtlpp::AccelerationStructure>()
#endif
	};

protected:
	FMetalResourceViewBase() = default;

public:
	virtual ~FMetalResourceViewBase();

	EMetalType GetMetalType() const
	{
		return static_cast<EMetalType>(Storage.GetIndex());
	}

	FMetalTexture const& GetTextureView() const
	{
		check(GetMetalType() == EMetalType::TextureView);
		return Storage.Get<FMetalTexture>();
	}

	FBufferView const& GetBufferView() const
	{
		check(GetMetalType() == EMetalType::BufferView);
		return Storage.Get<FBufferView>();
	}
    
    FTextureBufferBacked const& GetTextureBufferBacked() const
    {
        check(GetMetalType() == EMetalType::TextureBufferBacked);
        return Storage.Get<FTextureBufferBacked>();
    }

#if METAL_RHI_RAYTRACING
	mtlpp::AccelerationStructure const& GetAccelerationStructure() const
	{
		check(GetMetalType() == EMetalType::AccelerationStructure);
		return Storage.Get<mtlpp::AccelerationStructure>();
	}
#endif

	// TODO: This is kinda awkward; should probably be refactored at some point.
	TArray<TTuple<ns::AutoReleased<mtlpp::Resource>, mtlpp::ResourceUsage>> ReferencedResources;

	virtual void UpdateView() = 0;

protected:
	FMetalTexture& InitAsTextureView();
	void InitAsBufferView(FMetalBuffer& Buffer, uint32 Offset, uint32 Size);
    void InitAsTextureBufferBacked(FMetalTexture& Texture, FMetalBuffer& Buffer, uint32 Offset, uint32 Size, EPixelFormat Format);

	void Invalidate();

	bool bOwnsResource = true;

private:
	TStorage Storage;
};

class FMetalShaderResourceView final : public FRHIShaderResourceView, public FMetalResourceViewBase
{
public:
	FMetalShaderResourceView(FRHICommandListBase& RHICmdList, FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc);
	FMetalViewableResource* GetBaseResource() const;

	virtual void UpdateView() override;
};

class FMetalUnorderedAccessView final : public FRHIUnorderedAccessView, public FMetalResourceViewBase
{
public:
	FMetalUnorderedAccessView(FRHICommandListBase& RHICmdList, FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc);
	FMetalViewableResource* GetBaseResource() const;

	virtual void UpdateView() override;

	void ClearUAV(TRHICommandList_RecursiveHazardous<FMetalRHICommandContext>& RHICmdList, const void* ClearValue, bool bFloat);
#if UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
	void ClearUAVWithBlitEncoder(TRHICommandList_RecursiveHazardous<FMetalRHICommandContext>& RHICmdList, uint32 Pattern);
#endif
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
#if METAL_RHI_RAYTRACING
class FMetalRayTracingScene;
class FMetalRayTracingGeometry;
#endif // METAL_RHI_RAYTRACING

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
	typedef FMetalRHIBuffer TConcreteType;
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
#if METAL_RHI_RAYTRACING
template<>
struct TMetalResourceTraits<FRHIRayTracingScene>
{
	typedef FMetalRayTracingScene TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRayTracingGeometry>
{
	typedef FMetalRayTracingGeometry TConcreteType;
};
#endif // METAL_RHI_RAYTRACING
