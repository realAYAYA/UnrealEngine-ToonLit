// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include "mfobjects.h"
#include "d3d12.h"
#include "d3dx12.h"
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

#define	ELECTRA_MEDIAGPUBUFFER_DX12 1

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#ifdef _WIN64
#define ALLOW_USE_OF_SSEMEMCPY 1

#include <intrin.h>

static void SSE2MemCpy(const void* InDst, const void* InSrc, unsigned int InSize)
{
	__m128i* Dst = (__m128i*)InDst;
	__m128i* Src = (__m128i*)InSrc;

	InSize = InSize >> 7;
	for (unsigned int i = 0; i < InSize; i++)
	{
		_mm_prefetch((char*)(Src + 8), 0);
		_mm_prefetch((char*)(Src + 10), 0);
		_mm_prefetch((char*)(Src + 12), 0);
		_mm_prefetch((char*)(Src + 14), 0);
		__m128i m0 = _mm_load_si128(Src + 0);
		__m128i m1 = _mm_load_si128(Src + 1);
		__m128i m2 = _mm_load_si128(Src + 2);
		__m128i m3 = _mm_load_si128(Src + 3);
		__m128i m4 = _mm_load_si128(Src + 4);
		__m128i m5 = _mm_load_si128(Src + 5);
		__m128i m6 = _mm_load_si128(Src + 6);
		__m128i m7 = _mm_load_si128(Src + 7);
		_mm_stream_si128(Dst + 0, m0);
		_mm_stream_si128(Dst + 1, m1);
		_mm_stream_si128(Dst + 2, m2);
		_mm_stream_si128(Dst + 3, m3);
		_mm_stream_si128(Dst + 4, m4);
		_mm_stream_si128(Dst + 5, m5);
		_mm_stream_si128(Dst + 6, m6);
		_mm_stream_si128(Dst + 7, m7);
		Src += 8;
		Dst += 8;
	}
}

#else
#define ALLOW_USE_OF_SSEMEMCPY 0
#endif // _WIN64

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

class FElectraMediaDecoderOutputBufferPool_DX12;

// Simple DX12 heap and fence manager for use with decoder output buffers (both upload heaps or default GPU heaps)
class FElectraMediaDecoderOutputBufferPoolBlock_DX12
{
	struct FResourceInfo
	{
		FResourceInfo(TWeakPtr<FElectraMediaDecoderOutputBufferPool_DX12> InPool, uint32 InBlockIdx, uint32 InBufferIdx, TRefCountPtr<ID3D12Heap> InHeap) : Pool(InPool), BlockIdx(InBlockIdx), BufferIdx(InBufferIdx), Heap(InHeap) {}

		TWeakPtr<FElectraMediaDecoderOutputBufferPool_DX12> Pool;
		uint32 BlockIdx;
		uint32 BufferIdx;
		TRefCountPtr<ID3D12Heap> Heap;
	};

public:
	// Create instance for use with buffers (usually for uploading data)
	FElectraMediaDecoderOutputBufferPoolBlock_DX12(uint32 InBlockIdx, TRefCountPtr<ID3D12Device> InD3D12Device, uint32 InMaxNumBuffers, uint32 Width, uint32 Height, uint32 BytesPerPixel, D3D12_HEAP_TYPE InD3D12HeapType = D3D12_HEAP_TYPE_UPLOAD)
		: D3D12Device(InD3D12Device)
		, D3D12HeapType(InD3D12HeapType)
		, MaxNumBuffers(InMaxNumBuffers)
		, FreeMask((1 << InMaxNumBuffers) - 1)
		, BlockIdx(InBlockIdx)
	{
		check(InMaxNumBuffers <= 32);

		// Get the size needed per buffer
		BufferPitch = Align(Width * BytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		BufferSize = Align(BufferPitch * Height, FMath::Max(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

		InitCommon();
	}

	// Create instance for use with textures (usually to receive any upload buffers or internal decoder texture data)
	FElectraMediaDecoderOutputBufferPoolBlock_DX12(uint32 InBlockIdx, TRefCountPtr<ID3D12Device> InD3D12Device, uint32 InMaxNumBuffers, uint32 Width, uint32 Height, DXGI_FORMAT PixFmt, D3D12_HEAP_TYPE InD3D12HeapType = D3D12_HEAP_TYPE_DEFAULT)
		: D3D12Device(InD3D12Device)
		, D3D12HeapType(InD3D12HeapType)
		, MaxNumBuffers(InMaxNumBuffers)
		, FreeMask((1 << InMaxNumBuffers) - 1)
		, BlockIdx(InBlockIdx)
	{
		check(InMaxNumBuffers <= 32);

		// Get the size needed per buffer / texture
		D3D12_RESOURCE_DESC Desc = {};
		Desc.MipLevels = 1;
		Desc.Format = PixFmt;
		Desc.Width = Width;
		Desc.Height = Height;
		Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		Desc.DepthOrArraySize = 1;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		D3D12_RESOURCE_ALLOCATION_INFO AllocInfo = InD3D12Device->GetResourceAllocationInfo(0, 1, &Desc);

		BufferPitch = 0;
		BufferSize = Align(AllocInfo.SizeInBytes, AllocInfo.Alignment);
		InitCommon();
	}

	~FElectraMediaDecoderOutputBufferPoolBlock_DX12()
	{
	}

	// Check if the current setup is compatible with the new parameters
	bool IsCompatibleAsBuffer(uint32 InMaxNumBuffers, uint32 Width, uint32 Height, uint32 BytesPerPixel) const
	{
		uint32 NewBufferPitch = Align(Width * BytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		uint32 NewBufferSize = Align(NewBufferPitch * Height, FMath::Max(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
		return (MaxNumBuffers == InMaxNumBuffers && BufferSize == NewBufferSize && BufferPitch == NewBufferPitch);
	}

	// Check if a buffer is available
	bool BufferAvailable() const
	{
		return FPlatformAtomics::AtomicRead((const int32*)&FreeMask) != 0;
	}

	// Allocate a buffer resource and return suitable sync fence data
	TRefCountPtr<ID3D12Resource> AllocateOutputDataAsBuffer(TWeakPtr<FElectraMediaDecoderOutputBufferPool_DX12> InPoolWeakRef, uint32& OutPitch)
	{
		TRefCountPtr<ID3D12Resource> Resource = AllocateBuffer(InPoolWeakRef, OutPitch, [NumBytes = BufferSize](D3D12_RESOURCE_DESC& Desc)
		{
			Desc.MipLevels = 1;
			Desc.Format = DXGI_FORMAT_UNKNOWN;
			Desc.Width = NumBytes;
			Desc.Height = 1;
			Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			Desc.DepthOrArraySize = 1;
			Desc.SampleDesc.Count = 1;
			Desc.SampleDesc.Quality = 0;
			Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		});
		return Resource;
	}

	// Allocate a texture resource and return suitable sync fence data
	TRefCountPtr<ID3D12Resource> AllocateOutputDataAsTexture(TWeakPtr<FElectraMediaDecoderOutputBufferPool_DX12> InPoolWeakRef, uint32 InWidth, uint32 InHeight, DXGI_FORMAT InPixFmt)
	{
		uint32 OutPitch;
		TRefCountPtr<ID3D12Resource> Resource = AllocateBuffer(InPoolWeakRef, OutPitch, [InWidth, InHeight, InPixFmt](D3D12_RESOURCE_DESC& Desc)
		{
			Desc.MipLevels = 1;
			Desc.Format = InPixFmt;
			Desc.Width = InWidth;
			Desc.Height = InHeight;
			Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			Desc.DepthOrArraySize = 1;
			Desc.SampleDesc.Count = 1;
			Desc.SampleDesc.Quality = 0;
			Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		});
		return Resource;
	}

private:
	void InitCommon()
	{
		D3D12_HEAP_DESC HeapDesc = {};
		HeapDesc.Alignment = 0;
		HeapDesc.SizeInBytes = BufferSize * MaxNumBuffers;
		HeapDesc.Properties = CD3DX12_HEAP_PROPERTIES(D3D12HeapType);
		HeapDesc.Flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | ((D3D12HeapType == D3D12_HEAP_TYPE_UPLOAD) ? D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS : D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);
		HRESULT Res = D3D12Device->CreateHeap(&HeapDesc, IID_PPV_ARGS(D3D12OutputHeap.GetInitReference()));
		check(SUCCEEDED(Res));
		if (Res != S_OK)
		{
			FreeMask = 0;
		}
#if !UE_BUILD_SHIPPING
		if (D3D12OutputHeap)
		{
			D3D12OutputHeap->SetName(TEXT("ElectraOutputBufferPoolHeap"));
		}
#endif
	}

	TRefCountPtr<ID3D12Resource> AllocateBuffer(TWeakPtr<FElectraMediaDecoderOutputBufferPool_DX12> InPoolWeakRef, uint32& OutBufferPitch, TFunction<void(D3D12_RESOURCE_DESC& Desc)> && InitializeDesc)
	{
		TRefCountPtr<ID3D12Resource> Resource;

		// Find the buffer index we can use...
		int32 BufferIdx, OldFreeMask, NewFreeMask;
		do
		{
			OldFreeMask = (int32)FreeMask;
			if (OldFreeMask == 0)
			{
				BufferIdx = -1;
				break;
			}
			BufferIdx = 31 - FMath::CountLeadingZeros(OldFreeMask);
			NewFreeMask = OldFreeMask & ~(1 << BufferIdx);
		} while (OldFreeMask != FPlatformAtomics::InterlockedCompareExchange((int32*)&FreeMask, NewFreeMask, OldFreeMask));

		// Anything?
		if (BufferIdx >= 0)
		{
			// Make a placed resource to use...
			D3D12_RESOURCE_DESC Desc = {};
			InitializeDesc(Desc);
			HRESULT Res = D3D12Device->CreatePlacedResource(D3D12OutputHeap, BufferSize * BufferIdx, &Desc, (D3D12HeapType == D3D12_HEAP_TYPE_UPLOAD) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(Resource.GetInitReference()));
			check(SUCCEEDED(Res));
			if (Res == S_OK)
			{
#if !UE_BUILD_SHIPPING
				Resource->SetName(TEXT("ElectraOutputBufferPoolBufferResource"));
#endif

				OutBufferPitch = BufferPitch;

				// Register a destruction callback so we can reset our memory management here transparently...
				TRefCountPtr<ID3DDestructionNotifier> Notifier;
				Res = Resource->QueryInterface(__uuidof(ID3DDestructionNotifier), (void**)Notifier.GetInitReference());
				check(SUCCEEDED(Res));

				// note: we keep a reference to the heap in our context data for the destruction callback, so we can ensure the heap lives as long as there are placed resources in it
				UINT CallbackID;
				Res = Notifier->RegisterDestructionCallback(ResourceDestructionCallback, new FResourceInfo(InPoolWeakRef, BlockIdx, BufferIdx, D3D12OutputHeap), &CallbackID);
				check(SUCCEEDED(Res));
			}
			else
			{
				FreeBuffer(BufferIdx);
			}
		}

		return Resource;
	}

	void FreeBuffer(int32 InBufferIdx)
	{
		// Reset buffer index to free...
		int32 OldFreeMask, NewFreeMask;
		do
		{
			OldFreeMask = (int32)FreeMask;
			NewFreeMask = OldFreeMask | (1 << InBufferIdx);
		} while (OldFreeMask != FPlatformAtomics::InterlockedCompareExchange((int32*)&FreeMask, NewFreeMask, OldFreeMask));
	}

	static void ResourceDestructionCallback(void* Context);

	TRefCountPtr<ID3D12Device> D3D12Device;
	D3D12_HEAP_TYPE D3D12HeapType;
	TRefCountPtr<ID3D12Heap> D3D12OutputHeap;
	uint32 MaxNumBuffers;
	uint32 BufferSize;
	uint32 BufferPitch;
	uint32 FreeMask;
	uint32 BlockIdx;
};


class FElectraMediaDecoderOutputBufferPool_DX12 : public TSharedFromThis<FElectraMediaDecoderOutputBufferPool_DX12, ESPMode::ThreadSafe>
{
	enum {
		kExpectedMaxNumBlocks = 8,
	};
	
public:
	// Create instance for use with buffers (usually for uploading data)
	FElectraMediaDecoderOutputBufferPool_DX12(TRefCountPtr<ID3D12Device> InD3D12Device, uint32 InMaxNumBuffers, uint32 InWidth, uint32 InHeight, uint32 InBytesPerPixel, D3D12_HEAP_TYPE InD3D12HeapType = D3D12_HEAP_TYPE_UPLOAD)
		: D3D12Device(InD3D12Device)
		, LastFenceValue(0)
		, D3D12HeapType(InD3D12HeapType)
		, Width(InWidth)
		, Height(InHeight)
		, PixFmt(DXGI_FORMAT_UNKNOWN)
		, BytesPerPixel(InBytesPerPixel)
	{
		Blocks.Reserve(kExpectedMaxNumBlocks);
		Blocks.Emplace(new FElectraMediaDecoderOutputBufferPoolBlock_DX12(0, D3D12Device, InMaxNumBuffers, Width, Height, BytesPerPixel, D3D12HeapType));
		InitCommon(InMaxNumBuffers);
	}

	// Create instance for use with textures (usually to receive any upload buffers or internal decoder texture data)
	FElectraMediaDecoderOutputBufferPool_DX12(TRefCountPtr<ID3D12Device> InD3D12Device, uint32 InMaxNumBuffers, uint32 InWidth, uint32 InHeight, DXGI_FORMAT InPixFmt, D3D12_HEAP_TYPE InD3D12HeapType = D3D12_HEAP_TYPE_DEFAULT)
		: D3D12Device(InD3D12Device)
		, LastFenceValue(0)
		, D3D12HeapType(InD3D12HeapType)
		, Width(InWidth)
		, Height(InHeight)
		, PixFmt(InPixFmt)
		, BytesPerPixel(0)
	{
		Blocks.Reserve(kExpectedMaxNumBlocks);
		Blocks.Emplace(new FElectraMediaDecoderOutputBufferPoolBlock_DX12(0, D3D12Device, InMaxNumBuffers, Width, Height, PixFmt, D3D12HeapType));
		InitCommon(InMaxNumBuffers);
	}

	~FElectraMediaDecoderOutputBufferPool_DX12()
	{
		for (FElectraMediaDecoderOutputBufferPoolBlock_DX12* Block : Blocks)
		{
			delete Block;
		}
	}

	// Check if the current setup is compatible with the new parameters
	bool IsCompatibleAsBuffer(uint32 InMaxNumBuffers, uint32 InWidth, uint32 InHeight, uint32 InBytesPerPixel) const
	{
		return Blocks[0]->IsCompatibleAsBuffer(InMaxNumBuffers, InWidth, InHeight, InBytesPerPixel);
	}

	// Check if a buffer is available
	bool BufferAvailable() const
	{
		for (FElectraMediaDecoderOutputBufferPoolBlock_DX12* Block : Blocks)
		{
			if (Block->BufferAvailable())
			{
				return true;
			}
		}
		return false;
	}

	class FOutputData
	{
	public:
		~FOutputData()
		{
			check(ReadyToDestroy());
		}

		TRefCountPtr<ID3D12Resource> Resource;
		TRefCountPtr<ID3D12Fence> Fence;
		uint64 FenceValue;

		bool ReadyToDestroy() const
		{
			bool bOk = true;
#if !UE_BUILD_SHIPPING
			if (Resource.IsValid())
			{
				bOk = Resource->AddRef() > 1 || Fence->GetCompletedValue() >= FenceValue;
				Resource->Release();
			}
#endif
			return bOk;
		}
	};

	bool AllocateOutputDataAsBuffer(FOutputData& OutData, uint32& OutPitch)
	{
		check(BytesPerPixel != 0);

		OutData.Resource = nullptr;
		for (FElectraMediaDecoderOutputBufferPoolBlock_DX12* Block : Blocks)
		{
			if (Block->BufferAvailable())
			{
				OutData.Resource = Block->AllocateOutputDataAsBuffer(AsWeak(), OutPitch);
				if (OutData.Resource)
				{
					break;
				}
			}
		}

		if (!OutData.Resource.IsValid())
		{
			Blocks.Emplace(new FElectraMediaDecoderOutputBufferPoolBlock_DX12(Blocks.Num(), D3D12Device, MaxNumBuffersAddBlocks, Width, Height, BytesPerPixel, D3D12HeapType));
			OutData.Resource = Blocks.Last()->AllocateOutputDataAsBuffer(AsWeak(), OutPitch);
		}

		if (OutData.Resource.IsValid())
		{
			OutData.Fence = GetUpdatedBufferFence(OutData.FenceValue);
			return true;
		}
		return false;
	}

	bool AllocateOutputDataAsTexture(FOutputData& OutData, uint32 InWidth, uint32 InHeight, DXGI_FORMAT InPixFmt)
	{
		check(PixFmt != DXGI_FORMAT_UNKNOWN);

		OutData.Resource = nullptr;
		for (FElectraMediaDecoderOutputBufferPoolBlock_DX12* Block : Blocks)
		{
			if (Block->BufferAvailable())
			{
				OutData.Resource = Block->AllocateOutputDataAsTexture(AsWeak(), InWidth, InHeight, InPixFmt);
				if (OutData.Resource)
				{
					break;
				}
			}
		}

		if (!OutData.Resource.IsValid())
		{
			Blocks.Emplace(new FElectraMediaDecoderOutputBufferPoolBlock_DX12(Blocks.Num(), D3D12Device, MaxNumBuffersAddBlocks, Width, Height, PixFmt, D3D12HeapType));
			OutData.Resource = Blocks.Last()->AllocateOutputDataAsTexture(AsWeak(), InWidth, InHeight, InPixFmt);
		}

		if (OutData.Resource.IsValid())
		{
			OutData.Fence = GetUpdatedBufferFence(OutData.FenceValue);
			return true;
		}
		return false;
	}

	TRefCountPtr<ID3D12Fence> GetUpdatedBufferFence(uint64& FenceValue)
	{
		FenceValue = ++LastFenceValue;
		return D3D12BufferFence;
	}

	// Helper function to copy simple, linear texture data between differently picthed buffers
	static void CopyWithPitchAdjust(uint8* Dst, uint32 DstPitch, const uint8* Src, uint32 SrcPitch, uint32 NumRows)
	{
		if (DstPitch != SrcPitch)
		{
			for (uint32 Y = NumRows; Y > 0; --Y)
			{
				FMemory::Memcpy(Dst, Src, SrcPitch);
				Src += SrcPitch;
				Dst += DstPitch;
			}
		}
		else
		{
			uint32 NumBytes = SrcPitch * NumRows;
#if ALLOW_USE_OF_SSEMEMCPY
			if ((NumBytes & 0x7f) == 0)
			{
				SSE2MemCpy(Dst, Src, NumBytes);
			}
			else
#endif
			{
				FMemory::Memcpy(Dst, Src, NumBytes);
			}
		}
	}

private:
	friend class FElectraMediaDecoderOutputBufferPoolBlock_DX12;

	void InitCommon(uint32 InMaxNumBuffers)
	{
		MaxNumBuffersAddBlocks = (InMaxNumBuffers > 1) ? (InMaxNumBuffers >> 1) : 1;

		HRESULT Res = D3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(D3D12BufferFence.GetInitReference()));
		check(SUCCEEDED(Res));
#if !UE_BUILD_SHIPPING
		D3D12BufferFence->SetName(TEXT("ElectraOutputBufferPoolFence"));
#endif
	}

	TArray<FElectraMediaDecoderOutputBufferPoolBlock_DX12*> Blocks;
	
	TRefCountPtr<ID3D12Device> D3D12Device;
	TRefCountPtr<ID3D12Fence> D3D12BufferFence;
	uint64 LastFenceValue;
	D3D12_HEAP_TYPE D3D12HeapType;
	uint32 Width;
	uint32 Height;
	DXGI_FORMAT PixFmt;
	uint32 BytesPerPixel;
	uint32 MaxNumBuffersAddBlocks;
	};


inline void FElectraMediaDecoderOutputBufferPoolBlock_DX12::ResourceDestructionCallback(void* Context)
{
	auto ResourceInfo = reinterpret_cast<FResourceInfo*>(Context);

	if (auto Pool = ResourceInfo->Pool.Pin())
	{
		Pool->Blocks[ResourceInfo->BlockIdx]->FreeBuffer(ResourceInfo->BufferIdx);
	}

	// Get rid of our resource tracking info block...
	// (this will also release the ref to the heap the resource was on)
	delete ResourceInfo;
}


#include "CommonElectraDecoderGPUBufferHelpers.h"
