// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Util.h: D3D RHI utility definitions.
	=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "RenderUtils.h"

class FD3D12Adapter;

namespace D3D12RHI
{
	/**
	 * Dump & Log all the information we have on a GPU crash (NvAfterMath, DRED, Breadcrumbs, ...) and force quit
	 */
	extern void TerminateOnGPUCrash(ID3D12Device* InDevice, const void* InGPUCrashDump, const size_t InGPUCrashDumpSize);

	/**
	 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
	 * @param	Result - The result code to check
	 * @param	Code - The code which yielded the result.
	 * @param	Filename - The filename of the source file containing Code.
	 * @param	Line - The line number of Code within Filename.
	 */
	extern void VerifyD3D12Result(HRESULT Result, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, ID3D12Device* Device, FString Message = FString());

	/**
	* Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
	* @param	Result - The result code to check
	* @param	Code - The code which yielded the result.
	* @param	Filename - The filename of the source file containing Code.
	* @param	Line - The line number of Code within Filename.
	*/
	extern void VerifyD3D12CreateTextureResult(HRESULT D3DResult, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, const D3D12_RESOURCE_DESC& TextureDesc, ID3D12Device* Device);

	/**
	 * A macro for using VERIFYD3D12RESULT that automatically passes in the code and filename/line.
	 */
#define VERIFYD3D12RESULT_LAMBDA(x, Device, Lambda)	{HRESULT hres = x; if (FAILED(hres)) { VerifyD3D12Result(hres, #x, __FILE__, __LINE__, Device, Lambda()); }}
#define VERIFYD3D12RESULT_EX(x, Device)	{HRESULT hres = x; if (FAILED(hres)) { VerifyD3D12Result(hres, #x, __FILE__, __LINE__, Device); }}
#define VERIFYD3D12RESULT(x)			{HRESULT hres = x; if (FAILED(hres)) { VerifyD3D12Result(hres, #x, __FILE__, __LINE__, nullptr); }}
#define VERIFYD3D12CREATETEXTURERESULT(x, Desc, Device) {HRESULT hres = x; if (FAILED(hres)) { VerifyD3D12CreateTextureResult(hres, #x, __FILE__, __LINE__, Desc, Device); }}

	/**
	 * Checks that a COM object has the expected number of references.
	 */
	extern void VerifyComRefCount(IUnknown* Object, int32 ExpectedRefs, const TCHAR* Code, const TCHAR* Filename, int32 Line);
#define checkComRefCount(Obj,ExpectedRefs) VerifyComRefCount(Obj,ExpectedRefs,TEXT(#Obj),TEXT(__FILE__),__LINE__)

	/** Returns a string for the provided DXGI format. */
	const TCHAR* GetD3D12TextureFormatString(DXGI_FORMAT TextureFormat);

	/** Checks if given GPU virtual address corresponds to any known resource allocations and logs results */
	void LogPageFaultData(FD3D12Adapter* InAdapter, FD3D12Device* InDevice, D3D12_GPU_VIRTUAL_ADDRESS InPageFaultAddress);
	
} // namespace D3D12RHI

using namespace D3D12RHI;

class FD3D12Resource;

void SetName(ID3D12Object* const Object, const TCHAR* const Name);
void SetName(FD3D12Resource* const Resource, const TCHAR* const Name);

enum EShaderVisibility
{
	SV_Vertex,
	SV_Pixel,
	SV_Geometry,
#if PLATFORM_SUPPORTS_MESH_SHADERS
	SV_Mesh,
	SV_Amplification,
#endif
	SV_All,
	SV_ShaderVisibilityCount
};

enum ERTRootSignatureType
{
	RS_Raster,
	RS_RayTracingGlobal,
	RS_RayTracingLocal,
};

struct FShaderRegisterCounts
{
	uint8 SamplerCount;
	uint8 ConstantBufferCount;
	uint8 ShaderResourceCount;
	uint8 UnorderedAccessCount;
};

struct FD3D12QuantizedBoundShaderState
{
	FShaderRegisterCounts RegisterCounts[SV_ShaderVisibilityCount];
	ERTRootSignatureType RootSignatureType = RS_Raster;
	uint8 bAllowIAInputLayout : 1;
	uint8 bNeedsAgsIntrinsicsSpace : 1;
	uint8 bUseDiagnosticBuffer : 1;
	uint8 bUseDirectlyIndexedResourceHeap : 1;
	uint8 bUseDirectlyIndexedSamplerHeap : 1;
	uint8 Padding : 3;

	inline bool operator==(const FD3D12QuantizedBoundShaderState& RHS) const
	{
		return 0 == FMemory::Memcmp(this, &RHS, sizeof(RHS));
	}

	friend uint32 GetTypeHash(const FD3D12QuantizedBoundShaderState& Key);

	static void InitShaderRegisterCounts(D3D12_RESOURCE_BINDING_TIER ResourceBindingTier, const FShaderCodePackedResourceCounts& Counts, FShaderRegisterCounts& Shader, bool bAllowUAVs = false);
};

/**
* Creates a discrete bound shader state object from a collection of graphics pipeline shaders.
*/

class FD3D12BoundShaderState;
class FD3D12ComputeShader;

extern FD3D12QuantizedBoundShaderState QuantizeBoundGraphicsShaderState(FD3D12Adapter& Adapter, const FD3D12BoundShaderState* BSS);
extern FD3D12QuantizedBoundShaderState QuantizeBoundComputeShaderState(FD3D12Adapter& Adapter, const FD3D12ComputeShader* ComputeShader);

#if D3D12_RHI_RAYTRACING
class FD3D12RayTracingShader;
extern FD3D12QuantizedBoundShaderState QuantizeBoundRayTracingShaderState(FD3D12Adapter& Adapter, EShaderFrequency ShaderFrequency, const FD3D12RayTracingShader* Shader);
#endif

/**
* Convert from ECubeFace to D3DCUBEMAP_FACES type
* @param Face - ECubeFace type to convert
* @return D3D cube face enum value
*/
FORCEINLINE uint32 GetD3D12CubeFace(ECubeFace Face)
{
	switch (Face)
	{
	case CubeFace_PosX:
	default:
		return 0;//D3DCUBEMAP_FACE_POSITIVE_X;
	case CubeFace_NegX:
		return 1;//D3DCUBEMAP_FACE_NEGATIVE_X;
	case CubeFace_PosY:
		return 2;//D3DCUBEMAP_FACE_POSITIVE_Y;
	case CubeFace_NegY:
		return 3;//D3DCUBEMAP_FACE_NEGATIVE_Y;
	case CubeFace_PosZ:
		return 4;//D3DCUBEMAP_FACE_POSITIVE_Z;
	case CubeFace_NegZ:
		return 5;//D3DCUBEMAP_FACE_NEGATIVE_Z;
	};
}

/**
* Calculate a subresource index for a texture
*/
FORCEINLINE uint32 CalcSubresource(uint32 MipSlice, uint32 ArraySlice, uint32 MipLevels)
{
	return MipSlice + ArraySlice * MipLevels;
}

/**
 * Keeps track of Locks for D3D12 objects
 */
class FD3D12LockedKey
{
public:
	void* SourceObject;
	uint32 Subresource;

public:
	FD3D12LockedKey() : SourceObject(NULL)
		, Subresource(0)
	{}
	FD3D12LockedKey(FD3D12Resource* source, uint32 subres = 0) : SourceObject((void*)source)
		, Subresource(subres)
	{}
	FD3D12LockedKey(class FD3D12ResourceLocation* source, uint32 subres = 0) : SourceObject((void*)source)
		, Subresource(subres)
	{}

	template<class ClassType>
	FD3D12LockedKey(ClassType* source, uint32 subres = 0) : SourceObject((void*)source)
		, Subresource(subres)
	{}
	bool operator==(const FD3D12LockedKey& Other) const
	{
		return SourceObject == Other.SourceObject && Subresource == Other.Subresource;
	}
	bool operator!=(const FD3D12LockedKey& Other) const
	{
		return SourceObject != Other.SourceObject || Subresource != Other.Subresource;
	}
	FD3D12LockedKey& operator=(const FD3D12LockedKey& Other)
	{
		SourceObject = Other.SourceObject;
		Subresource = Other.Subresource;
		return *this;
	}
	uint32 GetHash() const
	{
		return PointerHash(SourceObject);
	}

	/** Hashing function. */
	friend uint32 GetTypeHash(const FD3D12LockedKey& K)
	{
		return K.GetHash();
	}
};

class FD3D12RenderTargetView;
class FD3D12DepthStencilView;

/**
 * Class for retrieving render targets currently bound to the device context.
 */
class FD3D12BoundRenderTargets
{
public:
	/** Initialization constructor: requires the state cache. */
	explicit FD3D12BoundRenderTargets(FD3D12RenderTargetView** RTArray, uint32 NumActiveRTs, FD3D12DepthStencilView* DSView);

	/** Destructor. */
	~FD3D12BoundRenderTargets();

	/** Accessors. */
	FORCEINLINE int32 GetNumActiveTargets() const { return NumActiveTargets; }
	FORCEINLINE FD3D12RenderTargetView* GetRenderTargetView(int32 TargetIndex) { return RenderTargetViews[TargetIndex]; }
	FORCEINLINE FD3D12DepthStencilView* GetDepthStencilView() { return DepthStencilView; }

private:
	/** Active render target views. */
	FD3D12RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];

	/** Active depth stencil view. */
	FD3D12DepthStencilView* DepthStencilView;

	/** The number of active render targets. */
	int32 NumActiveTargets;
};

void LogExecuteCommandLists(uint32 NumCommandLists, ID3D12CommandList *const *ppCommandLists);
FString ConvertToResourceStateString(uint32 ResourceState);
void LogResourceBarriers(TConstArrayView<D3D12_RESOURCE_BARRIER> Barriers, ID3D12CommandList *const pCommandList);

// Custom resource states
// To Be Determined (TBD) means we need to fill out a resource barrier before the command list is executed.
#define D3D12_RESOURCE_STATE_TBD D3D12_RESOURCE_STATES(-1 ^ (1 << 31))
#define D3D12_RESOURCE_STATE_CORRUPT D3D12_RESOURCE_STATES(-2 ^ (1 << 31))

static bool IsValidD3D12ResourceState(D3D12_RESOURCE_STATES InState)
{
	return (InState != D3D12_RESOURCE_STATE_TBD && InState != D3D12_RESOURCE_STATE_CORRUPT);
}

static bool IsDirectQueueExclusiveD3D12State(D3D12_RESOURCE_STATES InState)
{
	return EnumHasAnyFlags(InState, D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

D3D12_RESOURCE_STATES GetD3D12ResourceState(ERHIAccess InRHIAccess, bool InIsAsyncCompute);

//==================================================================================================================================
// CResourceState
// Tracking of per-resource or per-subresource state
//==================================================================================================================================
class CResourceState
{
public:
	void Initialize(uint32 SubresourceCount);

	bool AreAllSubresourcesSame() const;
	bool CheckResourceState(D3D12_RESOURCE_STATES State) const;
	bool CheckResourceStateInitalized() const;
	D3D12_RESOURCE_STATES GetSubresourceState(uint32 SubresourceIndex) const;
	bool CheckAllSubresourceSame();
	void SetResourceState(D3D12_RESOURCE_STATES State);
	void SetSubresourceState(uint32 SubresourceIndex, D3D12_RESOURCE_STATES State);

	D3D12_RESOURCE_STATES GetUAVHiddenResourceState() const
	{
		return static_cast<D3D12_RESOURCE_STATES>(UAVHiddenResourceState);
	}

	void SetUAVHiddenResourceState(D3D12_RESOURCE_STATES InUAVHiddenResourceState)
	{
		// The hidden state can never include UAV
		check(InUAVHiddenResourceState == D3D12_RESOURCE_STATE_TBD || !EnumHasAnyFlags(InUAVHiddenResourceState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		check((InUAVHiddenResourceState & (1 << 31)) == 0);

		UAVHiddenResourceState = (uint32)InUAVHiddenResourceState;
	}

	void SetHasInternalTransition()
	{
		bHasInternalTransition = 1;
	}
	bool HasInternalTransition() const
	{
		return bHasInternalTransition != 0;
	}

private:
	// Only used if m_AllSubresourcesSame is 1.
	// Bits defining the state of the full resource, bits are from D3D12_RESOURCE_STATES
	uint32 m_ResourceState : 31;

	// Set to 1 if m_ResourceState is valid.  In this case, all subresources have the same state
	// Set to 0 if m_SubresourceState is valid.  In this case, each subresources may have a different state (or may be unknown)
	uint32 m_AllSubresourcesSame : 1;

	// Special resource state to track previous state before resource transitioned to UAV when the resource
	// has a UAV aliasing resource so correct previous state can be found (only single state allowed)
	uint32 UAVHiddenResourceState : 31;

	// Was the resource used for another transition than the pending transition
	uint32 bHasInternalTransition : 1;

	// Only used if m_AllSubresourcesSame is 0.
	// The state of each subresources.  Bits are from D3D12_RESOURCE_STATES.
	TArray<D3D12_RESOURCE_STATES, TInlineAllocator<4>> m_SubresourceState;
};

//==================================================================================================================================
// FD3D12ShaderBytecode
// Encapsulates D3D12 shader bytecode and creates a hash for the shader bytecode
//==================================================================================================================================
struct ShaderBytecodeHash
{
	uint64 Hash[2];

	bool operator ==(const ShaderBytecodeHash &b) const
	{
		return (Hash[0] == b.Hash[0] && Hash[1] == b.Hash[1]);
	}

	bool operator !=(const ShaderBytecodeHash &b) const
	{
		return (Hash[0] != b.Hash[0] || Hash[1] != b.Hash[1]);
	}
};

/**
 * The base class of threadsafe reference counted objects.
 */
template <class Type>
struct FThreadsafeQueue
{
private:
	mutable FCriticalSection	SynchronizationObject; // made this mutable so this class can have const functions and still be thread safe
	TQueue<Type>				Items;
	uint32						Size = 0;
public:

	inline const uint32 GetSize() const { return Size; }

	void Enqueue(const Type& Item)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		Items.Enqueue(Item);
		Size++;
	}

	bool Dequeue(Type& Result)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		Size--;
		return Items.Dequeue(Result);
	}

	template <typename CompareFunc>
	bool Dequeue(Type& Result, const CompareFunc& Func)
	{
		FScopeLock ScopeLock(&SynchronizationObject);

		if (Items.Peek(Result))
		{
			if (Func(Result))
			{
				Items.Dequeue(Result);
				Size--;

				return true;
			}
		}

		return false;
	}

	template <typename ResultType, typename CompareFunc>
	bool BatchDequeue(TArray<ResultType>& Result, const CompareFunc& Func, uint32 MaxItems)
	{
		FScopeLock ScopeLock(&SynchronizationObject);

		uint32 i = 0;
		Type Item;
		while (Items.Peek(Item) && i <= MaxItems)
		{
			if (Func(Item))
			{
				Items.Dequeue(Item);
				Size--;
				Result.Emplace(Item);

				i++;
			}
			else
			{
				break;
			}
		}

		return i > 0;
	}

	bool Peek(Type& Result)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return Items.Peek(Result);
	}

	bool IsEmpty()
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return Items.IsEmpty();
	}

	void Empty()
	{
		FScopeLock ScopeLock(&SynchronizationObject);

		Type Result;
		while (Items.Dequeue(Result)) {}
	}
};

inline bool IsCPUWritable(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES *pCustomHeapProperties = nullptr)
{
	check(HeapType == D3D12_HEAP_TYPE_CUSTOM ? pCustomHeapProperties != nullptr : true);
	return HeapType == D3D12_HEAP_TYPE_UPLOAD ||
		(HeapType == D3D12_HEAP_TYPE_CUSTOM &&
			(pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE || pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK));
}

inline bool IsGPUOnly(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES *pCustomHeapProperties = nullptr)
{
	check(HeapType == D3D12_HEAP_TYPE_CUSTOM ? pCustomHeapProperties != nullptr : true);
	return HeapType == D3D12_HEAP_TYPE_DEFAULT ||
		(HeapType == D3D12_HEAP_TYPE_CUSTOM &&
		(pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE));
}

inline bool IsCPUAccessible(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES* pCustomHeapProperties = nullptr)
{
	return !IsGPUOnly(HeapType, pCustomHeapProperties);
}

inline D3D12_RESOURCE_STATES DetermineInitialResourceState(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES *pCustomHeapProperties = nullptr)
{
	if (HeapType == D3D12_HEAP_TYPE_DEFAULT || IsCPUWritable(HeapType, pCustomHeapProperties))
	{
		return D3D12_RESOURCE_STATE_GENERIC_READ;
	}
	else
	{
		check(HeapType == D3D12_HEAP_TYPE_READBACK);
		return D3D12_RESOURCE_STATE_COPY_DEST;
	}
}

static bool IsBlockCompressFormat(DXGI_FORMAT Format)
{
	// Returns true if BC1, BC2, BC3, BC4, BC5, BC6, BC7
	return (Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC5_SNORM) ||
		(Format >= DXGI_FORMAT_BC6H_TYPELESS && Format <= DXGI_FORMAT_BC7_UNORM_SRGB);
}

static inline uint64 GetTilesNeeded(uint32 Width, uint32 Height, uint32 Depth, const D3D12_TILE_SHAPE& Shape)
{
	return uint64((Width + Shape.WidthInTexels - 1) / Shape.WidthInTexels) *
		((Height + Shape.HeightInTexels - 1) / Shape.HeightInTexels) *
		((Depth + Shape.DepthInTexels - 1) / Shape.DepthInTexels);
}

static uint32 GetWidthAlignment(DXGI_FORMAT Format)
{
	switch (Format)
	{
	case DXGI_FORMAT_R8G8_B8G8_UNORM: return 2;
	case DXGI_FORMAT_G8R8_G8B8_UNORM: return 2;
	case DXGI_FORMAT_NV12: return 2;
	case DXGI_FORMAT_P010: return 2;
	case DXGI_FORMAT_P016: return 2;
	case DXGI_FORMAT_420_OPAQUE: return 2;
	case DXGI_FORMAT_YUY2: return 2;
	case DXGI_FORMAT_Y210: return 2;
	case DXGI_FORMAT_Y216: return 2;
	case DXGI_FORMAT_BC1_TYPELESS: return 4;
	case DXGI_FORMAT_BC1_UNORM: return 4;
	case DXGI_FORMAT_BC1_UNORM_SRGB: return 4;
	case DXGI_FORMAT_BC2_TYPELESS: return 4;
	case DXGI_FORMAT_BC2_UNORM: return 4;
	case DXGI_FORMAT_BC2_UNORM_SRGB: return 4;
	case DXGI_FORMAT_BC3_TYPELESS: return 4;
	case DXGI_FORMAT_BC3_UNORM: return 4;
	case DXGI_FORMAT_BC3_UNORM_SRGB: return 4;
	case DXGI_FORMAT_BC4_TYPELESS: return 4;
	case DXGI_FORMAT_BC4_UNORM: return 4;
	case DXGI_FORMAT_BC4_SNORM: return 4;
	case DXGI_FORMAT_BC5_TYPELESS: return 4;
	case DXGI_FORMAT_BC5_UNORM: return 4;
	case DXGI_FORMAT_BC5_SNORM: return 4;
	case DXGI_FORMAT_BC6H_TYPELESS: return 4;
	case DXGI_FORMAT_BC6H_UF16: return 4;
	case DXGI_FORMAT_BC6H_SF16: return 4;
	case DXGI_FORMAT_BC7_TYPELESS: return 4;
	case DXGI_FORMAT_BC7_UNORM: return 4;
	case DXGI_FORMAT_BC7_UNORM_SRGB: return 4;
	case DXGI_FORMAT_NV11: return 4;
	case DXGI_FORMAT_R1_UNORM: return 8;
	default: return 1;
	}
}

static uint32 GetHeightAlignment(DXGI_FORMAT Format)
{
	switch (Format)
	{
	case DXGI_FORMAT_NV12: return 2;
	case DXGI_FORMAT_P010: return 2;
	case DXGI_FORMAT_P016: return 2;
	case DXGI_FORMAT_420_OPAQUE: return 2;
	case DXGI_FORMAT_BC1_TYPELESS: return 4;
	case DXGI_FORMAT_BC1_UNORM: return 4;
	case DXGI_FORMAT_BC1_UNORM_SRGB: return 4;
	case DXGI_FORMAT_BC2_TYPELESS: return 4;
	case DXGI_FORMAT_BC2_UNORM: return 4;
	case DXGI_FORMAT_BC2_UNORM_SRGB: return 4;
	case DXGI_FORMAT_BC3_TYPELESS: return 4;
	case DXGI_FORMAT_BC3_UNORM: return 4;
	case DXGI_FORMAT_BC3_UNORM_SRGB: return 4;
	case DXGI_FORMAT_BC4_TYPELESS: return 4;
	case DXGI_FORMAT_BC4_UNORM: return 4;
	case DXGI_FORMAT_BC4_SNORM: return 4;
	case DXGI_FORMAT_BC5_TYPELESS: return 4;
	case DXGI_FORMAT_BC5_UNORM: return 4;
	case DXGI_FORMAT_BC5_SNORM: return 4;
	case DXGI_FORMAT_BC6H_TYPELESS: return 4;
	case DXGI_FORMAT_BC6H_UF16: return 4;
	case DXGI_FORMAT_BC6H_SF16: return 4;
	case DXGI_FORMAT_BC7_TYPELESS: return 4;
	case DXGI_FORMAT_BC7_UNORM: return 4;
	case DXGI_FORMAT_BC7_UNORM_SRGB: return 4;
	default: return 1;
	}
}

static void Get4KTileShape(D3D12_TILE_SHAPE* pTileShape, DXGI_FORMAT DXGIFormat, EPixelFormat UEFormat, D3D12_RESOURCE_DIMENSION Dimension, uint32 SampleCount)
{
	//Bits per unit
	uint32 BPU = GPixelFormats[UEFormat].BlockBytes * 8;

	switch (Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_BUFFER:
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
	{
		check(!IsBlockCompressFormat(DXGIFormat));
		pTileShape->WidthInTexels = (BPU == 0) ? 4096 : 4096 * 8 / BPU;
		pTileShape->HeightInTexels = 1;
		pTileShape->DepthInTexels = 1;
	}
	break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
	{
		pTileShape->DepthInTexels = 1;
		if (IsBlockCompressFormat(DXGIFormat))
		{
			// Currently only supported block sizes are 64 and 128.
			// These equations calculate the size in texels for a tile. It relies on the fact that 16*16*16 blocks fit in a tile if the block size is 128 bits.
			check(BPU == 64 || BPU == 128);
			pTileShape->WidthInTexels = 16 * GetWidthAlignment(DXGIFormat);
			pTileShape->HeightInTexels = 16 * GetHeightAlignment(DXGIFormat);
			if (BPU == 64)
			{
				// If bits per block are 64 we double width so it takes up the full tile size.
				// This is only true for BC1 and BC4
				check((DXGIFormat >= DXGI_FORMAT_BC1_TYPELESS && DXGIFormat <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
					(DXGIFormat >= DXGI_FORMAT_BC4_TYPELESS && DXGIFormat <= DXGI_FORMAT_BC4_SNORM));
				pTileShape->WidthInTexels *= 2;
			}
		}
		else
		{
			if (BPU <= 8)
			{
				pTileShape->WidthInTexels = 64;
				pTileShape->HeightInTexels = 64;
			}
			else if (BPU <= 16)
			{
				pTileShape->WidthInTexels = 64;
				pTileShape->HeightInTexels = 32;
			}
			else if (BPU <= 32)
			{
				pTileShape->WidthInTexels = 32;
				pTileShape->HeightInTexels = 32;
			}
			else if (BPU <= 64)
			{
				pTileShape->WidthInTexels = 32;
				pTileShape->HeightInTexels = 16;
			}
			else if (BPU <= 128)
			{
				pTileShape->WidthInTexels = 16;
				pTileShape->HeightInTexels = 16;
			}
			else
			{
				check(false);
			}

			if (SampleCount <= 1)
			{ /* Do nothing */
			}
			else if (SampleCount <= 2)
			{
				pTileShape->WidthInTexels /= 2;
				pTileShape->HeightInTexels /= 1;
			}
			else if (SampleCount <= 4)
			{
				pTileShape->WidthInTexels /= 2;
				pTileShape->HeightInTexels /= 2;
			}
			else if (SampleCount <= 8)
			{
				pTileShape->WidthInTexels /= 4;
				pTileShape->HeightInTexels /= 2;
			}
			else if (SampleCount <= 16)
			{
				pTileShape->WidthInTexels /= 4;
				pTileShape->HeightInTexels /= 4;
			}
			else
			{
				check(false);
			}

			check(GetWidthAlignment(DXGIFormat) == 1);
			check(GetHeightAlignment(DXGIFormat) == 1);
		}

		break;
	}
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
	{
		if (IsBlockCompressFormat(DXGIFormat))
		{
			// Currently only supported block sizes are 64 and 128.
			// These equations calculate the size in texels for a tile. It relies on the fact that 16*16*16 blocks fit in a tile if the block size is 128 bits.
			check(BPU == 64 || BPU == 128);
			pTileShape->WidthInTexels = 8 * GetWidthAlignment(DXGIFormat);
			pTileShape->HeightInTexels = 8 * GetHeightAlignment(DXGIFormat);
			pTileShape->DepthInTexels = 4;
			if (BPU == 64)
			{
				// If bits per block are 64 we double width so it takes up the full tile size.
				// This is only true for BC1 and BC4
				check((DXGIFormat >= DXGI_FORMAT_BC1_TYPELESS && DXGIFormat <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
					(DXGIFormat >= DXGI_FORMAT_BC4_TYPELESS && DXGIFormat <= DXGI_FORMAT_BC4_SNORM));
				pTileShape->DepthInTexels *= 2;
			}
		}
		else
		{
			if (BPU <= 8)
			{
				pTileShape->WidthInTexels = 16;
				pTileShape->HeightInTexels = 16;
				pTileShape->DepthInTexels = 16;
			}
			else if (BPU <= 16)
			{
				pTileShape->WidthInTexels = 16;
				pTileShape->HeightInTexels = 16;
				pTileShape->DepthInTexels = 8;
			}
			else if (BPU <= 32)
			{
				pTileShape->WidthInTexels = 16;
				pTileShape->HeightInTexels = 8;
				pTileShape->DepthInTexels = 8;
			}
			else if (BPU <= 64)
			{
				pTileShape->WidthInTexels = 8;
				pTileShape->HeightInTexels = 8;
				pTileShape->DepthInTexels = 8;
			}
			else if (BPU <= 128)
			{
				pTileShape->WidthInTexels = 8;
				pTileShape->HeightInTexels = 8;
				pTileShape->DepthInTexels = 4;
			}
			else
			{
				check(false);
			}

			check(GetWidthAlignment(DXGIFormat) == 1);
			check(GetHeightAlignment(DXGIFormat) == 1);
		}
	}
	break;
	}
}

template <class TView>
class FD3D12View;
class CViewSubresourceSubset;

template <class TView>
bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12View<TView>* pView, const D3D12_RESOURCE_STATES& State);

bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12Resource* pResource, const D3D12_RESOURCE_STATES& State, uint32 Subresource);
bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12Resource* pResource, const D3D12_RESOURCE_STATES& State, const CViewSubresourceSubset& SubresourceSubset);

FORCEINLINE_DEBUGGABLE D3D12_PRIMITIVE_TOPOLOGY_TYPE TranslatePrimitiveTopologyType(EPrimitiveTopologyType TopologyType)
{
	switch (TopologyType)
	{
	case EPrimitiveTopologyType::Triangle:	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case EPrimitiveTopologyType::Patch:		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	case EPrimitiveTopologyType::Line:		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case EPrimitiveTopologyType::Point:		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	default:
		ensure(0);
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	}
}

FORCEINLINE_DEBUGGABLE D3D_PRIMITIVE_TOPOLOGY TranslatePrimitiveType(EPrimitiveType PrimitiveType)
{
	switch (PrimitiveType)
	{
	case PT_TriangleList:				return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PT_TriangleStrip:				return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PT_LineList:					return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case PT_PointList:					return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	#if defined(D3D12RHI_PRIMITIVE_TOPOLOGY_RECTLIST)
		case PT_RectList:				return D3D12RHI_PRIMITIVE_TOPOLOGY_RECTLIST;
	#endif

	case PT_1_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
	case PT_2_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;
	case PT_3_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	case PT_4_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	case PT_5_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST;
	case PT_6_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST;
	case PT_7_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST;
	case PT_8_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST;
	case PT_9_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST;
	case PT_10_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST;
	case PT_11_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST;
	case PT_12_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST;
	case PT_13_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST;
	case PT_14_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST;
	case PT_15_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST;
	case PT_16_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
	case PT_17_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST;
	case PT_18_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST;
	case PT_19_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST;
	case PT_20_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST;
	case PT_21_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST;
	case PT_22_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST;
	case PT_23_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST;
	case PT_24_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST;
	case PT_25_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST;
	case PT_26_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST;
	case PT_27_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST;
	case PT_28_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST;
	case PT_29_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST;
	case PT_30_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST;
	case PT_31_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST;
	case PT_32_ControlPointPatchList:	return D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST;
	default:
		ensure(0);
		return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}
}

#pragma warning(push)
#pragma warning(disable: 4063)
FORCEINLINE_DEBUGGABLE D3D12_PRIMITIVE_TOPOLOGY_TYPE D3D12PrimitiveTypeToTopologyType(D3D_PRIMITIVE_TOPOLOGY PrimitiveType)
{
	switch (PrimitiveType)
	{
	case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

	case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
	case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	#if defined(D3D12RHI_PRIMITIVE_TOPOLOGY_RECTLIST)
		case D3D12RHI_PRIMITIVE_TOPOLOGY_RECTLIST:
			return D3D12RHI_PRIMITIVE_TOPOLOGY_TYPE_RECT;
	#endif

	case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;

	default:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	}
}
#pragma warning(pop)

// @return 0xffffffff if not not supported
FORCEINLINE_DEBUGGABLE uint32 GetMaxMSAAQuality(uint32 SampleCount)
{
	if (SampleCount <= DX_MAX_MSAA_COUNT)
	{
		// 0 has better quality (a more even distribution)
		// higher quality levels might be useful for non box filtered AA or when using weighted samples 
		return 0;
	}

	// not supported
	return 0xffffffff;
}

/** Find the appropriate depth-stencil typeless DXGI format for the given format. */
inline DXGI_FORMAT FindDepthStencilParentDXGIFormat(DXGI_FORMAT InFormat)
{
	switch (InFormat)
	{
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
		// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	};
	return InFormat;
}

static uint8 GetPlaneSliceFromViewFormat(DXGI_FORMAT ResourceFormat, DXGI_FORMAT ViewFormat)
{
	// Currently, the only planar resources used are depth-stencil formats
	switch (FindDepthStencilParentDXGIFormat(ResourceFormat))
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		switch (ViewFormat)
		{
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			return 0;
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return 1;
		}
		break;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		switch (ViewFormat)
		{
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			return 0;
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return 1;
		}
		break;
	case DXGI_FORMAT_NV12:
		switch (ViewFormat)
		{
		case DXGI_FORMAT_R8_UNORM:
			return 0;
		case DXGI_FORMAT_R8G8_UNORM:
			return 1;
		}
		break;
	}

	return 0;
}

static uint8 GetPlaneCount(DXGI_FORMAT Format)
{
	// Currently, the only planar resources used are depth-stencil formats
	// Note there is a D3D12 helper for this, D3D12GetFormatPlaneCount
	switch (FindDepthStencilParentDXGIFormat(Format))
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return 2;
	default:
		return 1;
	}
}

struct FD3D12ScopeLock
{
public:
	FD3D12ScopeLock(FCriticalSection* CritSec) : CS(CritSec) { CS->Lock(); }
	~FD3D12ScopeLock() { CS->Unlock(); }
private:
	FCriticalSection* CS;
};

struct FD3D12ScopeNoLock
{
public:
	FD3D12ScopeNoLock(FCriticalSection* CritSec) { /* Do Nothing! */ }
	~FD3D12ScopeNoLock() { /* Do Nothing! */ }
};