// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Math/Color.h"
#include "Containers/StaticArray.h"
#include "HAL/ThreadSafeCounter.h"
#include "Templates/RefCounting.h"
#include "PixelFormat.h"
#include "Async/TaskGraphFwd.h"
#include "RHIFwd.h"
#include "RHIImmutableSamplerState.h"
#include "RHITransition.h"
#include "MultiGPU.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/IntVector.h"
#include "Misc/SecureHash.h"

#include <atomic>

class FHazardPointerCollection;
class FRHIComputeCommandList;
class FRHICommandListImmediate;
class FRHITextureReference;
struct FClearValueBinding;
struct FRHIResourceInfo;
struct FGenerateMipsStruct;
enum class EClearBinding;

typedef TArray<FGraphEventRef, TInlineAllocator<4> > FGraphEventArray;


/** The base type of RHI resources. */
class FRHIResource
{
public:
	RHI_API FRHIResource(ERHIResourceType InResourceType);
	RHI_API virtual ~FRHIResource();

	FORCEINLINE_DEBUGGABLE uint32 AddRef() const
	{
		int32 NewValue = AtomicFlags.AddRef(std::memory_order_acquire);
		checkSlow(NewValue > 0); 
		return uint32(NewValue);
	}

private:
	// Separate function to avoid force inlining this everywhere. Helps both for code size and performance.
	RHI_API void Destroy() const;

public:
	FORCEINLINE_DEBUGGABLE uint32 Release() const
	{
		int32 NewValue = AtomicFlags.Release(std::memory_order_release);
		check(NewValue >= 0);

		if (NewValue == 0)
		{
			Destroy();
		}
		checkSlow(NewValue >= 0);
		return uint32(NewValue);
	}

	FORCEINLINE_DEBUGGABLE uint32 GetRefCount() const
	{
		int32 CurrentValue = AtomicFlags.GetNumRefs(std::memory_order_relaxed);
		checkSlow(CurrentValue >= 0); 
		return uint32(CurrentValue);
	}

	UE_DEPRECATED(5.3, "FlushPendingDeletes is deprecated, please use FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources)")
	RHI_API static int32 FlushPendingDeletes(FRHICommandListImmediate& RHICmdList);

	RHI_API static bool Bypass();

	bool IsValid() const
	{
		return AtomicFlags.IsValid(std::memory_order_relaxed);
	}

	void Delete()
	{
		verify(!AtomicFlags.MarkForDelete(std::memory_order_acquire));
		CurrentlyDeleting = this;
		delete this;
	}

	void DisableLifetimeExtension()
	{
		ensureMsgf(IsValid(), TEXT("Resource is already marked for deletion. This call is a no-op. DisableLifetimeExtension must be called while still holding a live reference."));
		bAllowExtendLifetime = false;
	}

	inline ERHIResourceType GetType() const { return ResourceType; }

	RHI_API FName GetOwnerName() const;
	RHI_API void SetOwnerName(const FName& InOwnerName);

#if RHI_ENABLE_RESOURCE_INFO
	// Get resource info if available.
	// Should return true if the ResourceInfo was filled with data.
	RHI_API virtual bool GetResourceInfo(FRHIResourceInfo& OutResourceInfo) const;

	static void BeginTrackingResource(FRHIResource* InResource);
	static void EndTrackingResource(FRHIResource* InResource);
	static void StartTrackingAllResources();
	static void StopTrackingAllResources();
#endif

private:
	class FAtomicFlags
	{
		static constexpr uint32 MarkedForDeleteBit    = 1 << 30;
		static constexpr uint32 DeletingBit           = 1 << 31;
		static constexpr uint32 NumRefsMask           = ~(MarkedForDeleteBit | DeletingBit);

		std::atomic_uint Packed = { 0 };

	public:
		int32 AddRef(std::memory_order MemoryOrder)
		{
			uint32 OldPacked = Packed.fetch_add(1, MemoryOrder);
			checkf((OldPacked & DeletingBit) == 0, TEXT("Resource is being deleted."));
			int32  NumRefs = (OldPacked & NumRefsMask) + 1;
			checkf(NumRefs < NumRefsMask, TEXT("Reference count has overflowed."));
			return NumRefs;
		}

		int32 Release(std::memory_order MemoryOrder)
		{
			uint32 OldPacked = Packed.fetch_sub(1, MemoryOrder);
			checkf((OldPacked & DeletingBit) == 0, TEXT("Resource is being deleted."));
			int32  NumRefs = (OldPacked & NumRefsMask) - 1;
			checkf(NumRefs >= 0, TEXT("Reference count has underflowed."));
			return NumRefs;
		}

		bool MarkForDelete(std::memory_order MemoryOrder)
		{
			uint32 OldPacked = Packed.fetch_or(MarkedForDeleteBit, MemoryOrder);
			check((OldPacked & DeletingBit) == 0);
			return (OldPacked & MarkedForDeleteBit) != 0;
		}

		bool UnmarkForDelete(std::memory_order MemoryOrder)
		{
			uint32 OldPacked = Packed.fetch_xor(MarkedForDeleteBit, MemoryOrder);
			check((OldPacked & DeletingBit) == 0);
			bool  OldMarkedForDelete = (OldPacked & MarkedForDeleteBit) != 0;
			check(OldMarkedForDelete == true);
			return OldMarkedForDelete;
		}

		bool Deleteing()
		{
			uint32 LocalPacked = Packed.load(std::memory_order_acquire);
			check((LocalPacked & MarkedForDeleteBit) != 0);
			check((LocalPacked & DeletingBit) == 0);
			uint32 NumRefs = LocalPacked & NumRefsMask;

			if (NumRefs == 0) // caches can bring dead objects back to life
			{
#if DO_CHECK
				Packed.fetch_or(DeletingBit, std::memory_order_acquire);
#endif
				return true;
			}
			else
			{
				UnmarkForDelete(std::memory_order_release);
				return false;
			}
		}

		bool IsValid(std::memory_order MemoryOrder)
		{
			uint32 LocalPacked = Packed.load(MemoryOrder);
			return (LocalPacked & MarkedForDeleteBit) == 0 && (LocalPacked & NumRefsMask) != 0;
		}

		bool IsMarkedForDelete(std::memory_order MemoryOrder)
		{
			return (Packed.load(MemoryOrder) & MarkedForDeleteBit) != 0;
		}

		int32 GetNumRefs(std::memory_order MemoryOrder)
		{
			return Packed.load(MemoryOrder) & NumRefsMask;
		}
	};
	mutable FAtomicFlags AtomicFlags;

	const ERHIResourceType ResourceType;
	uint8 bCommitted : 1;
	uint8 bAllowExtendLifetime : 1;
#if RHI_ENABLE_RESOURCE_INFO
	uint8 bBeingTracked : 1;
	FName OwnerName;
#endif

	RHI_API static FRHIResource* CurrentlyDeleting;

	friend FRHICommandListImmediate;
};

enum class EClearBinding
{
	ENoneBound, //no clear color associated with this target.  Target will not do hardware clears on most platforms
	EColorBound, //target has a clear color bound.  Clears will use the bound color, and do hardware clears.
	EDepthStencilBound, //target has a depthstencil value bound.  Clears will use the bound values and do hardware clears.
};

struct FClearValueBinding
{
	struct DSVAlue
	{
		float Depth;
		uint32 Stencil;
	};

	FClearValueBinding()
		: ColorBinding(EClearBinding::EColorBound)
	{
		Value.Color[0] = 0.0f;
		Value.Color[1] = 0.0f;
		Value.Color[2] = 0.0f;
		Value.Color[3] = 0.0f;
	}

	FClearValueBinding(EClearBinding NoBinding)
		: ColorBinding(NoBinding)
	{
		check(ColorBinding == EClearBinding::ENoneBound);

		Value.Color[0] = 0.0f;
		Value.Color[1] = 0.0f;
		Value.Color[2] = 0.0f;
		Value.Color[3] = 0.0f;

		Value.DSValue.Depth = 0.0f;
		Value.DSValue.Stencil = 0;
	}

	explicit FClearValueBinding(const FLinearColor& InClearColor)
		: ColorBinding(EClearBinding::EColorBound)
	{
		Value.Color[0] = InClearColor.R;
		Value.Color[1] = InClearColor.G;
		Value.Color[2] = InClearColor.B;
		Value.Color[3] = InClearColor.A;
	}

	explicit FClearValueBinding(float DepthClearValue, uint32 StencilClearValue = 0)
		: ColorBinding(EClearBinding::EDepthStencilBound)
	{
		Value.DSValue.Depth = DepthClearValue;
		Value.DSValue.Stencil = StencilClearValue;
	}

	FLinearColor GetClearColor() const
	{
		ensure(ColorBinding == EClearBinding::EColorBound);
		return FLinearColor(Value.Color[0], Value.Color[1], Value.Color[2], Value.Color[3]);
	}

	void GetDepthStencil(float& OutDepth, uint32& OutStencil) const
	{
		ensure(ColorBinding == EClearBinding::EDepthStencilBound);
		OutDepth = Value.DSValue.Depth;
		OutStencil = Value.DSValue.Stencil;
	}

	bool operator==(const FClearValueBinding& Other) const
	{
		if (ColorBinding == Other.ColorBinding)
		{
			if (ColorBinding == EClearBinding::EColorBound)
			{
				return
					Value.Color[0] == Other.Value.Color[0] &&
					Value.Color[1] == Other.Value.Color[1] &&
					Value.Color[2] == Other.Value.Color[2] &&
					Value.Color[3] == Other.Value.Color[3];

			}
			if (ColorBinding == EClearBinding::EDepthStencilBound)
			{
				return
					Value.DSValue.Depth == Other.Value.DSValue.Depth &&
					Value.DSValue.Stencil == Other.Value.DSValue.Stencil;
			}
			return true;
		}
		return false;
	}

	friend inline uint32 GetTypeHash(FClearValueBinding const& Binding)
	{
		uint32 Hash = GetTypeHash(Binding.ColorBinding);

		if (Binding.ColorBinding == EClearBinding::EColorBound)
		{
			Hash = HashCombine(Hash, GetTypeHash(Binding.Value.Color[0]));
			Hash = HashCombine(Hash, GetTypeHash(Binding.Value.Color[1]));
			Hash = HashCombine(Hash, GetTypeHash(Binding.Value.Color[2]));
			Hash = HashCombine(Hash, GetTypeHash(Binding.Value.Color[3]));
		}
		else if (Binding.ColorBinding == EClearBinding::EDepthStencilBound)
		{
			Hash = HashCombine(Hash, GetTypeHash(Binding.Value.DSValue.Depth  ));
			Hash = HashCombine(Hash, GetTypeHash(Binding.Value.DSValue.Stencil));
		}

		return Hash;
	}

	EClearBinding ColorBinding;

	union ClearValueType
	{
		float Color[4];
		DSVAlue DSValue;
	} Value;

	// common clear values
	static RHI_API const FClearValueBinding None;
	static RHI_API const FClearValueBinding Black;
	static RHI_API const FClearValueBinding BlackMaxAlpha;
	static RHI_API const FClearValueBinding White;
	static RHI_API const FClearValueBinding Transparent;
	static RHI_API const FClearValueBinding DepthOne;
	static RHI_API const FClearValueBinding DepthZero;
	static RHI_API const FClearValueBinding DepthNear;
	static RHI_API const FClearValueBinding DepthFar;	
	static RHI_API const FClearValueBinding Green;
	static RHI_API const FClearValueBinding DefaultNormal8Bit;
};

class FResourceBulkDataInterface;
class FResourceArrayInterface;

struct FRHIResourceCreateInfo
{
	FRHIResourceCreateInfo(const TCHAR* InDebugName)
		: BulkData(nullptr)
		, ResourceArray(nullptr)
		, ClearValueBinding(FLinearColor::Transparent)
		, GPUMask(FRHIGPUMask::All())
		, bWithoutNativeResource(false)
		, DebugName(InDebugName)
		, ExtData(0)
	{
		check(InDebugName);
	}

	// for CreateTexture calls
	FRHIResourceCreateInfo(const TCHAR* InDebugName, FResourceBulkDataInterface* InBulkData)
		: FRHIResourceCreateInfo(InDebugName)
	{
		BulkData = InBulkData;
	}

	// for CreateBuffer calls
	FRHIResourceCreateInfo(const TCHAR* InDebugName, FResourceArrayInterface* InResourceArray)
		: FRHIResourceCreateInfo(InDebugName)
	{
		ResourceArray = InResourceArray;
	}

	FRHIResourceCreateInfo(const TCHAR* InDebugName, const FClearValueBinding& InClearValueBinding)
		: FRHIResourceCreateInfo(InDebugName)
	{
		ClearValueBinding = InClearValueBinding;
	}

	FRHIResourceCreateInfo(uint32 InExtData)
		: FRHIResourceCreateInfo(TEXT(""))
	{
		ExtData = InExtData;
	}

	FName GetTraceClassName() const									{ const static FLazyName FRHIBufferName(TEXT("FRHIBuffer")); return (ClassName == NAME_None) ? FRHIBufferName : ClassName; }

	// for CreateTexture calls
	FResourceBulkDataInterface* BulkData;

	// for CreateBuffer calls
	FResourceArrayInterface* ResourceArray;

	// for binding clear colors to render targets.
	FClearValueBinding ClearValueBinding;

	// set of GPUs on which to create the resource
	FRHIGPUMask GPUMask;

	// whether to create an RHI object with no underlying resource
	bool bWithoutNativeResource;
	const TCHAR* DebugName;

	// optional data that would have come from an offline cooker or whatever - general purpose
	uint32 ExtData;

	FName ClassName = NAME_None;	// The owner class of FRHIBuffer used for Insight asset metadata tracing
	FName OwnerName = NAME_None;	// The owner name used for Insight asset metadata tracing
};

class FExclusiveDepthStencil
{
public:
	enum Type
	{
		// don't use those directly, use the combined versions below
		// 4 bits are used for depth and 4 for stencil to make the hex value readable and non overlapping
		DepthNop = 0x00,
		DepthRead = 0x01,
		DepthWrite = 0x02,
		DepthMask = 0x0f,
		StencilNop = 0x00,
		StencilRead = 0x10,
		StencilWrite = 0x20,
		StencilMask = 0xf0,

		// use those:
		DepthNop_StencilNop = DepthNop + StencilNop,
		DepthRead_StencilNop = DepthRead + StencilNop,
		DepthWrite_StencilNop = DepthWrite + StencilNop,
		DepthNop_StencilRead = DepthNop + StencilRead,
		DepthRead_StencilRead = DepthRead + StencilRead,
		DepthWrite_StencilRead = DepthWrite + StencilRead,
		DepthNop_StencilWrite = DepthNop + StencilWrite,
		DepthRead_StencilWrite = DepthRead + StencilWrite,
		DepthWrite_StencilWrite = DepthWrite + StencilWrite,
	};

private:
	Type Value;

public:
	// constructor
	FExclusiveDepthStencil(Type InValue = DepthNop_StencilNop)
		: Value(InValue)
	{
	}

	inline bool IsUsingDepthStencil() const
	{
		return Value != DepthNop_StencilNop;
	}
	inline bool IsUsingDepth() const
	{
		return (ExtractDepth() != DepthNop);
	}
	inline bool IsUsingStencil() const
	{
		return (ExtractStencil() != StencilNop);
	}
	inline bool IsDepthWrite() const
	{
		return ExtractDepth() == DepthWrite;
	}
	inline bool IsDepthRead() const
	{
		return ExtractDepth() == DepthRead;
	}
	inline bool IsStencilWrite() const
	{
		return ExtractStencil() == StencilWrite;
	}
	inline bool IsStencilRead() const
	{
		return ExtractStencil() == StencilRead;
	}

	inline bool IsAnyWrite() const
	{
		return IsDepthWrite() || IsStencilWrite();
	}

	inline void SetDepthWrite()
	{
		Value = (Type)(ExtractStencil() | DepthWrite);
	}
	inline void SetStencilWrite()
	{
		Value = (Type)(ExtractDepth() | StencilWrite);
	}
	inline void SetDepthStencilWrite(bool bDepth, bool bStencil)
	{
		Value = DepthNop_StencilNop;

		if (bDepth)
		{
			SetDepthWrite();
		}
		if (bStencil)
		{
			SetStencilWrite();
		}
	}
	bool operator==(const FExclusiveDepthStencil& rhs) const
	{
		return Value == rhs.Value;
	}

	bool operator != (const FExclusiveDepthStencil& RHS) const
	{
		return Value != RHS.Value;
	}

	inline bool IsValid(FExclusiveDepthStencil& Current) const
	{
		Type Depth = ExtractDepth();

		if (Depth != DepthNop && Depth != Current.ExtractDepth())
		{
			return false;
		}

		Type Stencil = ExtractStencil();

		if (Stencil != StencilNop && Stencil != Current.ExtractStencil())
		{
			return false;
		}

		return true;
	}

	inline void GetAccess(ERHIAccess& DepthAccess, ERHIAccess& StencilAccess) const
	{
		DepthAccess = ERHIAccess::None;

		// SRV access is allowed whilst a depth stencil target is "readable".
		constexpr ERHIAccess DSVReadOnlyMask =
			ERHIAccess::DSVRead;

		// If write access is required, only the depth block can access the resource.
		constexpr ERHIAccess DSVReadWriteMask =
			ERHIAccess::DSVRead |
			ERHIAccess::DSVWrite;

		if (IsUsingDepth())
		{
			DepthAccess = IsDepthWrite() ? DSVReadWriteMask : DSVReadOnlyMask;
		}

		StencilAccess = ERHIAccess::None;

		if (IsUsingStencil())
		{
			StencilAccess = IsStencilWrite() ? DSVReadWriteMask : DSVReadOnlyMask;
		}
	}

	template <typename TFunction>
	inline void EnumerateSubresources(TFunction Function) const
	{
		if (!IsUsingDepthStencil())
		{
			return;
		}

		ERHIAccess DepthAccess = ERHIAccess::None;
		ERHIAccess StencilAccess = ERHIAccess::None;
		GetAccess(DepthAccess, StencilAccess);

		// Same depth / stencil state; single subresource.
		if (DepthAccess == StencilAccess)
		{
			Function(DepthAccess, FRHITransitionInfo::kAllSubresources);
		}
		// Separate subresources for depth / stencil.
		else
		{
			if (DepthAccess != ERHIAccess::None)
			{
				Function(DepthAccess, FRHITransitionInfo::kDepthPlaneSlice);
			}
			if (StencilAccess != ERHIAccess::None)
			{
				Function(StencilAccess, FRHITransitionInfo::kStencilPlaneSlice);
			}
		}
	}

	/**
	* Returns a new FExclusiveDepthStencil to be used to transition a depth stencil resource to readable.
	* If the depth or stencil is already in a readable state, that particular component is returned as Nop,
	* to avoid unnecessary subresource transitions.
	*/
	inline FExclusiveDepthStencil GetReadableTransition() const
	{
		FExclusiveDepthStencil::Type NewDepthState = IsDepthWrite()
			? FExclusiveDepthStencil::DepthRead
			: FExclusiveDepthStencil::DepthNop;

		FExclusiveDepthStencil::Type NewStencilState = IsStencilWrite()
			? FExclusiveDepthStencil::StencilRead
			: FExclusiveDepthStencil::StencilNop;

		return (FExclusiveDepthStencil::Type)(NewDepthState | NewStencilState);
	}

	/**
	* Returns a new FExclusiveDepthStencil to be used to transition a depth stencil resource to readable.
	* If the depth or stencil is already in a readable state, that particular component is returned as Nop,
	* to avoid unnecessary subresource transitions.
	*/
	inline FExclusiveDepthStencil GetWritableTransition() const
	{
		FExclusiveDepthStencil::Type NewDepthState = IsDepthRead()
			? FExclusiveDepthStencil::DepthWrite
			: FExclusiveDepthStencil::DepthNop;

		FExclusiveDepthStencil::Type NewStencilState = IsStencilRead()
			? FExclusiveDepthStencil::StencilWrite
			: FExclusiveDepthStencil::StencilNop;

		return (FExclusiveDepthStencil::Type)(NewDepthState | NewStencilState);
	}

	uint32 GetIndex() const
	{
		// Note: The array to index has views created in that specific order.

		// we don't care about the Nop versions so less views are needed
		// we combine Nop and Write
		switch (Value)
		{
		case DepthWrite_StencilNop:
		case DepthNop_StencilWrite:
		case DepthWrite_StencilWrite:
		case DepthNop_StencilNop:
			return 0; // old DSAT_Writable

		case DepthRead_StencilNop:
		case DepthRead_StencilWrite:
			return 1; // old DSAT_ReadOnlyDepth

		case DepthNop_StencilRead:
		case DepthWrite_StencilRead:
			return 2; // old DSAT_ReadOnlyStencil

		case DepthRead_StencilRead:
			return 3; // old DSAT_ReadOnlyDepthAndStencil
		}
		// should never happen
		check(0);
		return -1;
	}
	static const uint32 MaxIndex = 4;

private:
	inline Type ExtractDepth() const
	{
		return (Type)(Value & DepthMask);
	}
	inline Type ExtractStencil() const
	{
		return (Type)(Value & StencilMask);
	}
	friend uint32 GetTypeHash(const FExclusiveDepthStencil& Ds);
};

//
// State blocks
//

class FRHISamplerState : public FRHIResource 
{
public:
	FRHISamplerState() : FRHIResource(RRT_SamplerState) {}
	virtual bool IsImmutable() const { return false; }
	virtual FRHIDescriptorHandle GetBindlessHandle() const { return FRHIDescriptorHandle(); }
};

class FRHIRasterizerState : public FRHIResource
{
public:
	FRHIRasterizerState() : FRHIResource(RRT_RasterizerState) {}
	virtual bool GetInitializer(struct FRasterizerStateInitializerRHI& Init) { return false; }
};

class FRHIDepthStencilState : public FRHIResource
{
public:
	FRHIDepthStencilState() : FRHIResource(RRT_DepthStencilState) {}
#if ENABLE_RHI_VALIDATION
	FExclusiveDepthStencil ActualDSMode;
#endif
	virtual bool GetInitializer(struct FDepthStencilStateInitializerRHI& Init) { return false; }
};

class FRHIBlendState : public FRHIResource
{
public:
	FRHIBlendState() : FRHIResource(RRT_BlendState) {}
	virtual bool GetInitializer(class FBlendStateInitializerRHI& Init) { return false; }
};

template <typename RHIState, typename RHIStateInitializer>
static bool MatchRHIState(RHIState* LHSState, RHIState* RHSState)
{
	RHIStateInitializer LHSStateInitializerRHI;
	RHIStateInitializer RHSStateInitializerRHI;
	if (LHSState)
	{
		LHSState->GetInitializer(LHSStateInitializerRHI);
	}
	if (RHSState)
	{
		RHSState->GetInitializer(RHSStateInitializerRHI);
	}
	return LHSStateInitializerRHI == RHSStateInitializerRHI;
}

//
// Shader bindings
//

typedef TArray<struct FVertexElement,TFixedAllocator<MaxVertexElementCount> > FVertexDeclarationElementList;

class FRHIVertexDeclaration : public FRHIResource
{
public:
	FRHIVertexDeclaration() : FRHIResource(RRT_VertexDeclaration) {}
	virtual bool GetInitializer(FVertexDeclarationElementList& Init) { return false; }
	virtual uint32 GetPrecachePSOHash() const { return 0; }
};

class FRHIBoundShaderState : public FRHIResource
{
public:
	FRHIBoundShaderState() : FRHIResource(RRT_BoundShaderState) {}
};

//
// Shaders
//

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define RHI_INCLUDE_SHADER_DEBUG_DATA 1
#else
	#define RHI_INCLUDE_SHADER_DEBUG_DATA 0
#endif

#if RHI_INCLUDE_SHADER_DEBUG_DATA
	#define RHI_IF_SHADER_DEBUG_DATA(...)	__VA_ARGS__
#else
	#define RHI_IF_SHADER_DEBUG_DATA(...)
#endif

class FRHIShader : public FRHIResource
{
public:
	void SetHash(FSHAHash InHash) { Hash = InHash; }
	FSHAHash GetHash() const { return Hash; }

#if RHI_INCLUDE_SHADER_DEBUG_DATA

	// for debugging only e.g. MaterialName:ShaderFile.usf or ShaderFile.usf/EntryFunc
	struct
	{
		FString ShaderName;
		TArray<FName> UniformBufferNames;
	} Debug;

	const TCHAR* GetShaderName() const
	{
		return Debug.ShaderName.Len()
			? *Debug.ShaderName
			: TEXT("<unknown>");
	}

	FString GetUniformBufferName(uint32 Index) const
	{
		return Debug.UniformBufferNames.IsValidIndex(Index)
			? Debug.UniformBufferNames[Index].ToString()
			: TEXT("<unknown>");
	}

	TArray<FShaderCodeValidationStride> DebugStrideValidationData;
	TArray<FShaderCodeValidationType> DebugSRVTypeValidationData;
	TArray<FShaderCodeValidationType> DebugUAVTypeValidationData;
	TArray<FShaderCodeValidationUBSize> DebugUBSizeValidationData;

#else

	const TCHAR* GetShaderName() const { return TEXT("<unknown>"); }
	FString GetUniformBufferName(uint32 Index) const { return TEXT("<unknown>"); }

#endif

	FRHIShader() = delete;
	FRHIShader(ERHIResourceType InResourceType, EShaderFrequency InFrequency)
		: FRHIResource(InResourceType)
		, Frequency(InFrequency)
		, bNoDerivativeOps(false)
		, bHasShaderBundleUsage(false)
	{
	}

	inline EShaderFrequency GetFrequency() const
	{
		return Frequency;
	}

	inline void SetNoDerivativeOps(bool bValue)
	{
		bNoDerivativeOps = bValue;
	}

	inline bool HasNoDerivativeOps() const
	{
		return bNoDerivativeOps;
	}

	inline void SetShaderBundleUsage(bool bValue)
	{
		bHasShaderBundleUsage = bValue;
	}

	inline bool HasShaderBundleUsage() const
	{
		return bHasShaderBundleUsage;
	}

private:
	FSHAHash Hash;
	EShaderFrequency Frequency;
	uint8 bNoDerivativeOps : 1;
	uint8 bHasShaderBundleUsage : 1;
};

class FRHIGraphicsShader : public FRHIShader
{
public:
	explicit FRHIGraphicsShader(ERHIResourceType InResourceType, EShaderFrequency InFrequency)
		: FRHIShader(InResourceType, InFrequency) {}
};

class FRHIVertexShader : public FRHIGraphicsShader
{
public:
	FRHIVertexShader() : FRHIGraphicsShader(RRT_VertexShader, SF_Vertex) {}
};

class FRHIMeshShader : public FRHIGraphicsShader
{
public:
	FRHIMeshShader() : FRHIGraphicsShader(RRT_MeshShader, SF_Mesh) {}
};

class FRHIAmplificationShader : public FRHIGraphicsShader
{
public:
	FRHIAmplificationShader() : FRHIGraphicsShader(RRT_AmplificationShader, SF_Amplification) {}
};

class FRHIPixelShader : public FRHIGraphicsShader
{
public:
	FRHIPixelShader() : FRHIGraphicsShader(RRT_PixelShader, SF_Pixel) {}
};

class FRHIGeometryShader : public FRHIGraphicsShader
{
public:
	FRHIGeometryShader() : FRHIGraphicsShader(RRT_GeometryShader, SF_Geometry) {}
};

class FRHIRayTracingShader : public FRHIShader
{
public:
	explicit FRHIRayTracingShader(EShaderFrequency InFrequency) : FRHIShader(RRT_RayTracingShader, InFrequency) {}

	uint32 RayTracingPayloadType = 0; // This corresponds to the ERayTracingPayloadType enum associated with the shader
	uint32 RayTracingPayloadSize = 0; // The (maximum) size of the payload associated with this shader
};

class FRHIRayGenShader : public FRHIRayTracingShader
{
public:
	FRHIRayGenShader() : FRHIRayTracingShader(SF_RayGen) {}
};

class FRHIRayMissShader : public FRHIRayTracingShader
{
public:
	FRHIRayMissShader() : FRHIRayTracingShader(SF_RayMiss) {}
};

class FRHIRayCallableShader : public FRHIRayTracingShader
{
public:
	FRHIRayCallableShader() : FRHIRayTracingShader(SF_RayCallable) {}
};

class FRHIRayHitGroupShader : public FRHIRayTracingShader
{
public:
	FRHIRayHitGroupShader() : FRHIRayTracingShader(SF_RayHitGroup) {}
};

class FRHIComputeShader : public FRHIShader
{
public:
	FRHIComputeShader() : FRHIShader(RRT_ComputeShader, SF_Compute)
	, Stats(nullptr)
	{
	}
	
	inline void SetStats(struct FPipelineStateStats* Ptr) { Stats = Ptr; }
	RHI_API void UpdateStats();

private:
	struct FPipelineStateStats* Stats;
};

//
// Pipeline States
//

class FRHIGraphicsPipelineState : public FRHIResource 
{
public:
	FRHIGraphicsPipelineState() : FRHIResource(RRT_GraphicsPipelineState) {}

	inline void SetSortKey(uint64 InSortKey) { SortKey = InSortKey; }
	inline uint64 GetSortKey() const { return SortKey; }

private:
	uint64 SortKey = 0;

#if ENABLE_RHI_VALIDATION
	friend class FValidationContext;
	friend class FValidationRHI;
	FExclusiveDepthStencil DSMode;
#endif
};

class FRHIComputePipelineState : public FRHIResource
{
public:
	FRHIComputePipelineState() : FRHIResource(RRT_ComputePipelineState) {}

	inline void SetValid(bool InIsValid) { bIsValid = InIsValid; }
	inline bool IsValid() const { return bIsValid; }

private:
	bool bIsValid = true;
};

class FRHIRayTracingPipelineState : public FRHIResource
{
public:
	FRHIRayTracingPipelineState() : FRHIResource(RRT_RayTracingPipelineState) {}
};

//
// Buffers
//

// Whether to assert in cases where the layout is released before uniform buffers created with that layout
#define VALIDATE_UNIFORM_BUFFER_LAYOUT_LIFETIME 0

// Whether to assert when a uniform buffer is being deleted while still referenced by a mesh draw command
// Enabling this requires -norhithread to work correctly since FRHIResource lifetime is managed by both the RT and RHIThread
#define VALIDATE_UNIFORM_BUFFER_LIFETIME 0

struct FRHIUniformBufferResource
{
	/** Byte offset to each resource in the uniform buffer memory. */
	uint16 MemberOffset;

	/** Type of the member that allow (). */
	EUniformBufferBaseType MemberType;

	/** Compare two uniform buffer layout resources. */
	friend inline bool operator==(const FRHIUniformBufferResource& A, const FRHIUniformBufferResource& B)
	{
		return A.MemberOffset == B.MemberOffset
			&& A.MemberType == B.MemberType;
	}
};

inline constexpr uint16 kUniformBufferInvalidOffset = TNumericLimits<uint16>::Max();

struct FRHIUniformBufferLayoutInitializer;

/** The layout of a uniform buffer in memory. */
struct FRHIUniformBufferLayout : public FRHIResource
{
	FRHIUniformBufferLayout() = delete;

	RHI_API explicit FRHIUniformBufferLayout(const FRHIUniformBufferLayoutInitializer& Initializer);

	inline const FString& GetDebugName() const
	{
		return Name;
	}

	inline uint32 GetHash() const
	{
		checkSlow(Hash != 0);
		return Hash;
	}

	inline bool HasRenderTargets() const
	{
		return RenderTargetsOffset != kUniformBufferInvalidOffset;
	}

	inline bool HasExternalOutputs() const
	{
		return bHasNonGraphOutputs;
	}

	inline bool HasStaticSlot() const
	{
		return IsUniformBufferStaticSlotValid(StaticSlot);
	}

	const FString Name;

	/** The list of all resource inlined into the shader parameter structure. */
	const TArray<FRHIUniformBufferResource> Resources;

	/** The list of all RDG resource references inlined into the shader parameter structure. */
	const TArray<FRHIUniformBufferResource> GraphResources;

	/** The list of all RDG texture references inlined into the shader parameter structure. */
	const TArray<FRHIUniformBufferResource> GraphTextures;

	/** The list of all RDG buffer references inlined into the shader parameter structure. */
	const TArray<FRHIUniformBufferResource> GraphBuffers;

	/** The list of all RDG uniform buffer references inlined into the shader parameter structure. */
	const TArray<FRHIUniformBufferResource> GraphUniformBuffers;

	/** The list of all non-RDG uniform buffer references inlined into the shader parameter structure. */
	const TArray<FRHIUniformBufferResource> UniformBuffers;

	const uint32 Hash;

	/** The size of the constant buffer in bytes. */
	const uint32 ConstantBufferSize;

	/** The render target binding slots offset, if it exists. */
	const uint16 RenderTargetsOffset;

	/** The static slot (if applicable). */
	const FUniformBufferStaticSlot StaticSlot;

	/** The binding flags describing how this resource can be bound to the RHI. */
	const EUniformBufferBindingFlags BindingFlags;

	/** Whether this layout may contain non-render-graph outputs (e.g. RHI UAVs). */
	const bool bHasNonGraphOutputs;

	/** Used for platforms which use emulated ub's, forces a real uniform buffer instead */
	const bool bNoEmulatedUniformBuffer;

	/** This struct is a view into uniform buffer object, on platforms that support UBO */
	const bool bUniformView;

	/** Compare two uniform buffer layouts. */
	friend inline bool operator==(const FRHIUniformBufferLayout& A, const FRHIUniformBufferLayout& B)
	{
		return A.ConstantBufferSize == B.ConstantBufferSize
			&& A.StaticSlot == B.StaticSlot
			&& A.BindingFlags == B.BindingFlags
			&& A.Resources == B.Resources;
	}
};

class FRHIUniformBuffer : public FRHIResource
#if ENABLE_RHI_VALIDATION
	, public RHIValidation::FUniformBufferResource
#endif
{
public:
	FRHIUniformBuffer() = delete;

	/** Initialization constructor. */
	FRHIUniformBuffer(const FRHIUniformBufferLayout* InLayout)
	: FRHIResource(RRT_UniformBuffer)
	, Layout(InLayout)
	, LayoutConstantBufferSize(InLayout->ConstantBufferSize)
	{}

	FORCEINLINE_DEBUGGABLE uint32 Release() const
	{
		const FRHIUniformBufferLayout* LocalLayout = Layout;

#if VALIDATE_UNIFORM_BUFFER_LIFETIME
		int32 LocalNumMeshCommandReferencesForDebugging = NumMeshCommandReferencesForDebugging;
#endif

		uint32 NewRefCount = FRHIResource::Release();

		if (NewRefCount == 0)
		{
#if VALIDATE_UNIFORM_BUFFER_LIFETIME
			check(LocalNumMeshCommandReferencesForDebugging == 0 || IsEngineExitRequested());
#endif
		}

		return NewRefCount;
	}

	/** @return The number of bytes in the uniform buffer. */
	uint32 GetSize() const
	{
		check(LayoutConstantBufferSize == Layout->ConstantBufferSize);
		return LayoutConstantBufferSize;
	}
	const FRHIUniformBufferLayout& GetLayout() const { return *Layout; }
	const FRHIUniformBufferLayout* GetLayoutPtr() const { return Layout; }

#if VALIDATE_UNIFORM_BUFFER_LIFETIME
	mutable std::atomic<int32> NumMeshCommandReferencesForDebugging = 0;
#endif

	const TArray<TRefCountPtr<FRHIResource>>& GetResourceTable() const { return ResourceTable; }

protected:
	TArray<TRefCountPtr<FRHIResource>> ResourceTable;

private:
	/** Layout of the uniform buffer. */
	TRefCountPtr<const FRHIUniformBufferLayout> Layout;

	uint32 LayoutConstantBufferSize;
};

class FRHIViewableResource : public FRHIResource
{
public:
	// TODO (RemoveUnknowns) remove once FRHIBufferCreateDesc contains initial access.
	void SetTrackedAccess_Unsafe(ERHIAccess Access)
	{
		TrackedAccess = Access;
	}

	FName GetName() const
	{
		return Name;
	}

#if ENABLE_RHI_VALIDATION
	virtual RHIValidation::FResource* GetValidationTrackerResource() = 0;
#endif

protected:
	FRHIViewableResource(ERHIResourceType InResourceType, ERHIAccess InAccess)
		: FRHIResource(InResourceType)
		, TrackedAccess(InAccess)
	{}

	void TakeOwnership(FRHIViewableResource& Other)
	{
		TrackedAccess = Other.TrackedAccess;
	}

	void ReleaseOwnership()
	{
		TrackedAccess = ERHIAccess::Unknown;
	}

	FName Name;

private:
	ERHIAccess TrackedAccess;

	friend class FRHIComputeCommandList;
	friend class IRHIComputeContext;
};

struct FRHIBufferDesc
{
	uint32 Size{};
	uint32 Stride{};
	EBufferUsageFlags Usage{};

	FRHIBufferDesc() = default;
	FRHIBufferDesc(uint32 InSize, uint32 InStride, EBufferUsageFlags InUsage)
		: Size  (InSize)
		, Stride(InStride)
		, Usage (InUsage)
	{}

	static FRHIBufferDesc Null()
	{
		return FRHIBufferDesc(0, 0, BUF_NullResource);
	}

	bool IsNull() const
	{
		if (EnumHasAnyFlags(Usage, BUF_NullResource))
		{
			// The null resource descriptor should have its other fields zeroed, and no additional flags.
			check(Size == 0 && Stride == 0 && Usage == BUF_NullResource);
			return true;
		}

		return false;
	}
};

class FRHIBuffer : public FRHIViewableResource
#if ENABLE_RHI_VALIDATION
	, public RHIValidation::FBufferResource
#endif
{
public:
	/** Initialization constructor. */
	FRHIBuffer(FRHIBufferDesc const& InDesc)
		: FRHIViewableResource(RRT_Buffer, ERHIAccess::Unknown /* TODO (RemoveUnknowns): Use InitialAccess from descriptor after refactor. */)
		, Desc(InDesc)
	{}

	FRHIBufferDesc const& GetDesc() const { return Desc; }

	/** @return The number of bytes in the buffer. */
	uint32 GetSize() const { return Desc.Size; }

	/** @return The stride in bytes of the buffer. */
	uint32 GetStride() const { return Desc.Stride; }

	/** @return The usage flags used to create the buffer. */
	EBufferUsageFlags GetUsage() const { return Desc.Usage; }

	void SetName(const FName& InName) { Name = InName; }

	virtual uint32 GetParentGPUIndex() const { return 0; }

#if ENABLE_RHI_VALIDATION
	virtual RHIValidation::FResource* GetValidationTrackerResource() final override
	{
		return this;
	}
#endif

protected:
	// Used by RHI implementations that may adjust internal usage flags during object construction.
	void SetUsage(EBufferUsageFlags InUsage)
	{
		Desc.Usage = InUsage;
	}

	void TakeOwnership(FRHIBuffer& Other)
	{
		FRHIViewableResource::TakeOwnership(Other);
		Desc = Other.Desc;
	}

	void ReleaseOwnership()
	{
		FRHIViewableResource::ReleaseOwnership();
		Desc = FRHIBufferDesc::Null();
	}

private:
	FRHIBufferDesc Desc;
};

//
// Textures
//

class FLastRenderTimeContainer
{
public:
	FLastRenderTimeContainer() : LastRenderTime(-FLT_MAX) {}

	double GetLastRenderTime() const { return LastRenderTime; }

	void SetLastRenderTime(double InLastRenderTime) 
	{ 
		// avoid dirty caches from redundant writes
		if (LastRenderTime != InLastRenderTime)
		{
			LastRenderTime = InLastRenderTime;
		}
	}

private:
	/** The last time the resource was rendered. */
	double LastRenderTime;
};


/** Descriptor used to create a texture resource */
struct FRHITextureDesc
{
	FRHITextureDesc() = default;

	FRHITextureDesc(const FRHITextureDesc& Other)
	{
		*this = Other;
	}

	FRHITextureDesc(ETextureDimension InDimension)
		: Dimension(InDimension)
	{}

	FRHITextureDesc(
		  ETextureDimension   InDimension
		, ETextureCreateFlags InFlags
		, EPixelFormat        InFormat
		, FClearValueBinding  InClearValue
		, FIntPoint           InExtent
		, uint16              InDepth
		, uint16              InArraySize
		, uint8               InNumMips
		, uint8               InNumSamples
		, uint32              InExtData
		)
		: Flags     (InFlags     )
		, ClearValue(InClearValue)
		, ExtData   (InExtData   )
		, Extent    (InExtent    )
		, Depth     (InDepth     )
		, ArraySize (InArraySize )
		, NumMips   (InNumMips   )
		, NumSamples(InNumSamples)
		, Dimension (InDimension )
		, Format    (InFormat    )
	{}

	friend uint32 GetTypeHash(const FRHITextureDesc& Desc)
	{
		uint32 Hash = GetTypeHash(Desc.Dimension);
		Hash = HashCombine(Hash, GetTypeHash(Desc.Flags		));
		Hash = HashCombine(Hash, GetTypeHash(Desc.Format	));
		Hash = HashCombine(Hash, GetTypeHash(Desc.UAVFormat	));
		Hash = HashCombine(Hash, GetTypeHash(Desc.Extent	));
		Hash = HashCombine(Hash, GetTypeHash(Desc.Depth		));
		Hash = HashCombine(Hash, GetTypeHash(Desc.ArraySize	));
		Hash = HashCombine(Hash, GetTypeHash(Desc.NumMips	));
		Hash = HashCombine(Hash, GetTypeHash(Desc.NumSamples));
		Hash = HashCombine(Hash, GetTypeHash(Desc.FastVRAMPercentage));
		Hash = HashCombine(Hash, GetTypeHash(Desc.ClearValue));
		Hash = HashCombine(Hash, GetTypeHash(Desc.ExtData   ));
		Hash = HashCombine(Hash, GetTypeHash(Desc.GPUMask.GetNative()));
		return Hash;
	}

	bool operator == (const FRHITextureDesc& Other) const
	{
		return Dimension  == Other.Dimension
			&& Flags      == Other.Flags
			&& Format     == Other.Format
			&& UAVFormat  == Other.UAVFormat
			&& Extent     == Other.Extent
			&& Depth      == Other.Depth
			&& ArraySize  == Other.ArraySize
			&& NumMips    == Other.NumMips
			&& NumSamples == Other.NumSamples
			&& FastVRAMPercentage == Other.FastVRAMPercentage
			&& ClearValue == Other.ClearValue
			&& ExtData    == Other.ExtData
			&& GPUMask    == Other.GPUMask;
	}

	bool operator != (const FRHITextureDesc& Other) const
	{
		return !(*this == Other);
	}

	FRHITextureDesc& operator=(const FRHITextureDesc& Other)
	{
		Dimension			= Other.Dimension;
		Flags				= Other.Flags;
		Format				= Other.Format;
		UAVFormat			= Other.UAVFormat;
		Extent				= Other.Extent;
		Depth				= Other.Depth;
		ArraySize			= Other.ArraySize;
		NumMips				= Other.NumMips;
		NumSamples			= Other.NumSamples;
		ClearValue			= Other.ClearValue;
		ExtData				= Other.ExtData;
		FastVRAMPercentage	= Other.FastVRAMPercentage;
		GPUMask				= Other.GPUMask;

		return *this;
	}

	bool IsTexture2D() const
	{
		return Dimension == ETextureDimension::Texture2D || Dimension == ETextureDimension::Texture2DArray;
	}

	bool IsTexture3D() const
	{
		return Dimension == ETextureDimension::Texture3D;
	}

	bool IsTextureCube() const
	{
		return Dimension == ETextureDimension::TextureCube || Dimension == ETextureDimension::TextureCubeArray;
	}

	bool IsTextureArray() const
	{
		return Dimension == ETextureDimension::Texture2DArray || Dimension == ETextureDimension::TextureCubeArray;
	}

	bool IsMipChain() const
	{
		return NumMips > 1;
	}

	bool IsMultisample() const
	{
		return NumSamples > 1;
	}

	FIntVector GetSize() const
	{
		return FIntVector(Extent.X, Extent.Y, Depth);
	}

	void Reset()
	{
		// Usually we don't want to propagate MSAA samples.
		NumSamples = 1;

		// Remove UAV flag for textures that don't need it (some formats are incompatible).
		Flags |= TexCreate_RenderTargetable;
		Flags &= ~(TexCreate_UAV | TexCreate_ResolveTargetable | TexCreate_DepthStencilResolveTarget | TexCreate_Memoryless);
	}

	/** Returns whether this descriptor conforms to requirements. */
	bool IsValid() const
	{
		return FRHITextureDesc::Validate(*this, /* Name = */ TEXT(""), /* bFatal = */ false);
	}

	/** Texture flags passed on to RHI texture. */
	ETextureCreateFlags Flags = TexCreate_None;

	/** Clear value to use when fast-clearing the texture. */
	FClearValueBinding ClearValue;

	/* A mask representing which GPUs to create the resource on, in a multi-GPU system. */
	FRHIGPUMask GPUMask = FRHIGPUMask::All();

	/** Platform-specific additional data. Used for offline processed textures on some platforms. */
	uint32 ExtData = 0;

	/** Extent of the texture in x and y. */
	FIntPoint Extent = FIntPoint(1, 1);

	/** Depth of the texture if the dimension is 3D. */
	uint16 Depth = 1;

	/** The number of array elements in the texture. (Keep at 1 if dimension is 3D). */
	uint16 ArraySize = 1;

	/** Number of mips in the texture mip-map chain. */
	uint8 NumMips = 1;

	/** Number of samples in the texture. >1 for MSAA. */
	uint8 NumSamples = 1;

	/** Texture dimension to use when creating the RHI texture. */
	ETextureDimension Dimension = ETextureDimension::Texture2D;

	/** Pixel format used to create RHI texture. */
	EPixelFormat Format = PF_Unknown;

	/** Texture format used when creating the UAV. PF_Unknown means to use the default one (same as Format). */
	EPixelFormat UAVFormat = PF_Unknown;

	/** Resource memory percentage which should be allocated onto fast VRAM (hint-only). (encoding into 8bits, 0..255 -> 0%..100%) */
	uint8 FastVRAMPercentage = 0xFF;

	/** Check the validity. */
	static bool CheckValidity(const FRHITextureDesc& Desc, const TCHAR* Name)
	{
		return FRHITextureDesc::Validate(Desc, Name, /* bFatal = */ true);
	}

	/**
	 * Returns an estimated total memory size the described texture will occupy in GPU memory.
	 * This is an estimate because it only considers the dimensions / format etc of the texture, 
	 * not any specifics about platform texture layout.
	 * 
	 * To get a true measure of a texture resource for the current running platform RHI, use RHICalcTexturePlatformSize().
	 * 
	 * @param FirstMipIndex - the index of the most detailed mip to consider in the memory size calculation. Must be < NumMips and <= LastMipIndex.
	 * @param LastMipIndex  - the index of the least detailed mip to consider in the memory size calculation. Must be < NumMips and >= FirstMipIndex.
	 */
	RHI_API uint64 CalcMemorySizeEstimate(uint32 FirstMipIndex, uint32 LastMipIndex) const;

	uint64 CalcMemorySizeEstimate(uint32 FirstMipIndex = 0) const
	{
		return CalcMemorySizeEstimate(FirstMipIndex, NumMips - 1);
	}

private:
	RHI_API static bool Validate(const FRHITextureDesc& Desc, const TCHAR* Name, bool bFatal);
};

// @todo deprecate
using FRHITextureCreateInfo = FRHITextureDesc;

extern RHI_API ERHIAccess RHIGetDefaultResourceState(ETextureCreateFlags InUsage, bool bInHasInitialData);

struct FRHITextureCreateDesc : public FRHITextureDesc
{
	static FRHITextureCreateDesc Create(const TCHAR* InDebugName, ETextureDimension InDimension)
	{
		return FRHITextureCreateDesc(InDebugName, InDimension);
	}

	static FRHITextureCreateDesc Create2D(const TCHAR* InDebugName)
	{
		return FRHITextureCreateDesc(InDebugName, ETextureDimension::Texture2D);
	}

	static FRHITextureCreateDesc Create2DArray(const TCHAR* InDebugName)
	{
		return FRHITextureCreateDesc(InDebugName, ETextureDimension::Texture2DArray);
	}

	static FRHITextureCreateDesc Create3D(const TCHAR* InDebugName)
	{
		return FRHITextureCreateDesc(InDebugName, ETextureDimension::Texture3D);
	}

	static FRHITextureCreateDesc CreateCube(const TCHAR* InDebugName)
	{
		return FRHITextureCreateDesc(InDebugName, ETextureDimension::TextureCube);
	}

	static FRHITextureCreateDesc CreateCubeArray(const TCHAR* InDebugName)
	{
		return FRHITextureCreateDesc(InDebugName, ETextureDimension::TextureCubeArray);
	}

	static FRHITextureCreateDesc Create2D(const TCHAR* DebugName, FIntPoint Size, EPixelFormat Format)
	{
		return Create2D(DebugName)
			.SetExtent(Size)
			.SetFormat(Format);
	}

	static FRHITextureCreateDesc Create2D(const TCHAR* DebugName, int32 SizeX, int32 SizeY, EPixelFormat Format)
	{
		return Create2D(DebugName)
			.SetExtent(SizeX, SizeY)
			.SetFormat(Format);
	}

	static FRHITextureCreateDesc Create2DArray(const TCHAR* DebugName, FIntPoint Size, uint16 ArraySize, EPixelFormat Format)
	{
		return Create2DArray(DebugName)
			.SetExtent(Size)
			.SetFormat(Format)
			.SetArraySize((uint16)ArraySize);
	}

	static FRHITextureCreateDesc Create2DArray(const TCHAR* DebugName, int32 SizeX, int32 SizeY, int32 ArraySize, EPixelFormat Format)
	{
		return Create2DArray(DebugName)
			.SetExtent(SizeX, SizeY)
			.SetFormat(Format)
			.SetArraySize((uint16)ArraySize);
	}

	static FRHITextureCreateDesc Create3D(const TCHAR* DebugName, FIntVector Size, EPixelFormat Format)
	{
		return Create3D(DebugName)
			.SetExtent(Size.X, Size.Y)
			.SetDepth((uint16)Size.Z)
			.SetFormat(Format);
	}

	static FRHITextureCreateDesc Create3D(const TCHAR* DebugName, int32 SizeX, int32 SizeY, int32 SizeZ, EPixelFormat Format)
	{
		return Create3D(DebugName)
			.SetExtent(SizeX, SizeY)
			.SetDepth((uint16)SizeZ)
			.SetFormat(Format);
	}

	static FRHITextureCreateDesc CreateCube(const TCHAR* DebugName, uint32 Size, EPixelFormat Format)
	{
		return CreateCube(DebugName)
			.SetExtent(Size)
			.SetFormat(Format);
	}

	static FRHITextureCreateDesc CreateCubeArray(const TCHAR* DebugName, uint32 Size, uint16 ArraySize, EPixelFormat Format)
	{
		return CreateCubeArray(DebugName)
			.SetExtent(Size)
			.SetFormat(Format)
			.SetArraySize((uint16)ArraySize);
	}

	FRHITextureCreateDesc() = default;

	// Constructor with minimal argument set. Name and dimension are always required.
	FRHITextureCreateDesc(const TCHAR* InDebugName, ETextureDimension InDimension)
		: FRHITextureDesc(InDimension)
		, DebugName(InDebugName)
	{
	}

	// Constructor for when you already have an FRHITextureDesc
	FRHITextureCreateDesc(
		  FRHITextureDesc const&      InDesc
		, ERHIAccess                  InInitialState
		, TCHAR const*                InDebugName
		, FResourceBulkDataInterface* InBulkData     = nullptr
		)
		: FRHITextureDesc(InDesc)
		, InitialState   (InInitialState)
		, DebugName      (InDebugName)
		, BulkData       (InBulkData)
	{}

	void CheckValidity() const
	{
		FRHITextureDesc::CheckValidity(*this, DebugName);

		ensureMsgf(InitialState != ERHIAccess::Unknown, TEXT("Resource %s cannot be created in an unknown state."), DebugName);
	}

	FRHITextureCreateDesc& SetFlags(ETextureCreateFlags InFlags)               { Flags = InFlags;                          return *this; }
	FRHITextureCreateDesc& AddFlags(ETextureCreateFlags InFlags)               { Flags |= InFlags;                         return *this; }
	FRHITextureCreateDesc& SetClearValue(FClearValueBinding InClearValue)      { ClearValue = InClearValue;                return *this; }
	FRHITextureCreateDesc& SetExtData(uint32 InExtData)                        { ExtData = InExtData;                      return *this; }
	FRHITextureCreateDesc& SetExtent(const FIntPoint& InExtent)                { Extent = InExtent;                        return *this; }
	FRHITextureCreateDesc& SetExtent(int32 InExtentX, int32 InExtentY)         { Extent = FIntPoint(InExtentX, InExtentY); return *this; }
	FRHITextureCreateDesc& SetExtent(uint32 InExtent)                          { Extent = FIntPoint(InExtent);             return *this; }
	FRHITextureCreateDesc& SetDepth(uint16 InDepth)                            { Depth = InDepth;                          return *this; }
	FRHITextureCreateDesc& SetArraySize(uint16 InArraySize)                    { ArraySize = InArraySize;                  return *this; }
	FRHITextureCreateDesc& SetNumMips(uint8 InNumMips)                         { NumMips = InNumMips;                      return *this; }
	FRHITextureCreateDesc& SetNumSamples(uint8 InNumSamples)                   { NumSamples = InNumSamples;                return *this; }
	FRHITextureCreateDesc& SetDimension(ETextureDimension InDimension)         { Dimension = InDimension;                  return *this; }
	FRHITextureCreateDesc& SetFormat(EPixelFormat InFormat)                    { Format = InFormat;                        return *this; }
	FRHITextureCreateDesc& SetUAVFormat(EPixelFormat InUAVFormat)              { UAVFormat = InUAVFormat;                  return *this; }
	FRHITextureCreateDesc& SetInitialState(ERHIAccess InInitialState)          { InitialState = InInitialState;            return *this; }
	FRHITextureCreateDesc& SetDebugName(const TCHAR* InDebugName)              { DebugName = InDebugName;                  return *this; }
	FRHITextureCreateDesc& SetGPUMask(FRHIGPUMask InGPUMask)                   { GPUMask = InGPUMask;                      return *this; }
	FRHITextureCreateDesc& SetBulkData(FResourceBulkDataInterface* InBulkData) { BulkData = InBulkData;                    return *this; }
	FRHITextureCreateDesc& DetermineInititialState()                           { if (InitialState == ERHIAccess::Unknown) InitialState = RHIGetDefaultResourceState(Flags, BulkData != nullptr); return *this; }
	FRHITextureCreateDesc& SetFastVRAMPercentage(float In)                     { FastVRAMPercentage = uint8(FMath::Clamp(In, 0.f, 1.0f) * 0xFF); return *this; }
	FRHITextureCreateDesc& SetClassName(const FName& InClassName)			   { ClassName = InClassName;				   return *this; }
	FRHITextureCreateDesc& SetOwnerName(const FName& InOwnerName)			   { OwnerName = InOwnerName;                  return *this; }
	FName GetTraceClassName() const											   { const static FLazyName FRHITextureName(TEXT("FRHITexture")); return (ClassName == NAME_None) ? FRHITextureName : ClassName; }

	/* The RHI access state that the resource will be created in. */
	ERHIAccess InitialState = ERHIAccess::Unknown;

	/* A friendly name for the resource. */
	const TCHAR* DebugName = nullptr;

	/* Optional initial data to fill the resource with. */
	FResourceBulkDataInterface* BulkData = nullptr;

	FName ClassName = NAME_None;	// The owner class of FRHITexture used for Insight asset metadata tracing
	FName OwnerName = NAME_None;	// The owner name used for Insight asset metadata tracing
};

class FRHITexture : public FRHIViewableResource
#if ENABLE_RHI_VALIDATION
	, public RHIValidation::FTextureResource
#endif
{
protected:
	/** Initialization constructor. Should only be called by platform RHI implementations. */
	RHI_API FRHITexture(const FRHITextureCreateDesc& InDesc);

public:
	/**
	 * Get the texture description used to create the texture
	 * Still virtual because FRHITextureReference can override this function - remove virtual when FRHITextureReference is deprecated
	 *
	 * @return TextureDesc used to create the texture
	 */
	virtual const FRHITextureDesc& GetDesc() const { return TextureDesc; }
	
	///
	/// Virtual functions implemented per RHI
	/// 
	
	virtual class FRHITextureReference* GetTextureReference() { return NULL; }
	virtual FRHIDescriptorHandle GetDefaultBindlessHandle() const { return FRHIDescriptorHandle(); }

	/**
	 * Returns access to the platform-specific native resource pointer.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeResource() const
	{
		// Override this in derived classes to expose access to the native texture resource
		return nullptr;
	}

	/**
	 * Returns access to the platform-specific native shader resource view pointer.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeShaderResourceView() const
	{
		// Override this in derived classes to expose access to the native texture resource
		return nullptr;
	}

	/**
	 * Returns access to the platform-specific RHI texture baseclass.  This is designed to provide the RHI with fast access to its base classes in the face of multiple inheritance.
	 * @return	The pointer to the platform-specific RHI texture baseclass or NULL if it not initialized or not supported for this RHI
	 */
	virtual void* GetTextureBaseRHI()
	{
		// Override this in derived classes to expose access to the native texture resource
		return nullptr;
	}

	virtual void GetWriteMaskProperties(void*& OutData, uint32& OutSize)
	{
		OutData = nullptr;
		OutSize = 0;
	}

	///
	/// Helper getter functions - non virtual
	/// 

	/**
	 * Returns the x, y & z dimensions if the texture
	 * The Z component will always be 1 for 2D/cube resources and will contain depth for volume textures & array size for array textures
	 */
	FIntVector GetSizeXYZ() const
	{
		const FRHITextureDesc& Desc = GetDesc();
		switch (Desc.Dimension)
		{
		case ETextureDimension::Texture2D:		  return FIntVector(Desc.Extent.X, Desc.Extent.Y, 1);
		case ETextureDimension::Texture2DArray:	  return FIntVector(Desc.Extent.X, Desc.Extent.Y, Desc.ArraySize);
		case ETextureDimension::Texture3D:		  return FIntVector(Desc.Extent.X, Desc.Extent.Y, Desc.Depth);
		case ETextureDimension::TextureCube:	  return FIntVector(Desc.Extent.X, Desc.Extent.Y, 1);
		case ETextureDimension::TextureCubeArray: return FIntVector(Desc.Extent.X, Desc.Extent.Y, Desc.ArraySize);
		}
		return FIntVector(0, 0, 0);
	}

	/**
	 * Returns the dimensions (i.e. the actual number of texels in each dimension) of the specified mip. ArraySize is ignored.
	 * The Z component will always be 1 for 2D/cube resources and will contain depth for volume textures.
	 * This differs from GetSizeXYZ() which returns ArraySize in Z for 2D arrays.
	 */
	FIntVector GetMipDimensions(uint8 MipIndex) const
	{
		const FRHITextureDesc& Desc = GetDesc();
		return FIntVector(
			FMath::Max<int32>(Desc.Extent.X >> MipIndex, 1),
			FMath::Max<int32>(Desc.Extent.Y >> MipIndex, 1),
			FMath::Max<int32>(Desc.Depth    >> MipIndex, 1)
		);
	}

	/** @return Whether the texture is multi sampled. */
	bool IsMultisampled() const { return GetDesc().NumSamples > 1; }

	/** @return Whether the texture has a clear color defined */
	bool HasClearValue() const
	{
		return GetDesc().ClearValue.ColorBinding != EClearBinding::ENoneBound;
	}

	/** @return the clear color value if set */
	FLinearColor GetClearColor() const
	{
		return GetDesc().ClearValue.GetClearColor();
	}

	/** @return the depth & stencil clear value if set */
	void GetDepthStencilClearValue(float& OutDepth, uint32& OutStencil) const
	{
		return GetDesc().ClearValue.GetDepthStencil(OutDepth, OutStencil);
	}

	/** @return the depth clear value if set */
	float GetDepthClearValue() const
	{
		float Depth;
		uint32 Stencil;
		GetDesc().ClearValue.GetDepthStencil(Depth, Stencil);
		return Depth;
	}

	/** @return the stencil clear value if set */
	uint32 GetStencilClearValue() const
	{
		float Depth;
		uint32 Stencil;
		GetDesc().ClearValue.GetDepthStencil(Depth, Stencil);
		return Stencil;
	}

	///
	/// RenderTime & Name functions - non virtual
	/// 

	/** sets the last time this texture was cached in a resource table. */
	FORCEINLINE_DEBUGGABLE void SetLastRenderTime(float InLastRenderTime)
	{
		LastRenderTime.SetLastRenderTime(InLastRenderTime);
	}

	double GetLastRenderTime() const
	{
		return LastRenderTime.GetLastRenderTime();
	}

	RHI_API void SetName(const FName& InName);

	///
	/// Deprecated functions
	/// 

	//UE_DEPRECATED(5.1, "FRHITexture2D is deprecated, please use FRHITexture directly")
	inline FRHITexture2D* GetTexture2D() { return TextureDesc.Dimension == ETextureDimension::Texture2D ? this : nullptr; }
	//UE_DEPRECATED(5.1, "FRHITexture2DArray is deprecated, please use FRHITexture directly")
	inline FRHITexture2DArray* GetTexture2DArray() { return TextureDesc.Dimension == ETextureDimension::Texture2DArray ? this : nullptr; }
	//UE_DEPRECATED(5.1, "FRHITexture3D is deprecated, please use FRHITexture directly")
	inline FRHITexture3D* GetTexture3D() { return TextureDesc.Dimension == ETextureDimension::Texture3D ? this : nullptr; }
	//UE_DEPRECATED(5.1, "FRHITextureCube is deprecated, please use FRHITexture directly")
	inline FRHITextureCube* GetTextureCube() { return TextureDesc.IsTextureCube() ? this : nullptr; }

	//UE_DEPRECATED(5.1, "GetSizeX() is deprecated, please use GetDesc().Extent.X instead")
	uint32 GetSizeX() const { return GetDesc().Extent.X; }

	//UE_DEPRECATED(5.1, "GetSizeY() is deprecated, please use GetDesc().Extent.Y instead")
	uint32 GetSizeY() const { return GetDesc().Extent.Y; }

	//UE_DEPRECATED(5.1, "GetSizeXY() is deprecated, please use GetDesc().Extent.X or GetDesc().Extent.Y instead")
	FIntPoint GetSizeXY() const { return FIntPoint(GetDesc().Extent.X, GetDesc().Extent.Y); }

	//UE_DEPRECATED(5.1, "GetSizeZ() is deprecated, please use GetDesc().ArraySize instead for TextureArrays and GetDesc().Depth for 3D textures")
	uint32 GetSizeZ() const { return GetSizeXYZ().Z; }

	//UE_DEPRECATED(5.1, "GetNumMips() is deprecated, please use GetDesc().NumMips instead")
	uint32 GetNumMips() const { return GetDesc().NumMips; }

	//UE_DEPRECATED(5.1, "GetFormat() is deprecated, please use GetDesc().Format instead")
	EPixelFormat GetFormat() const { return GetDesc().Format; }

	//UE_DEPRECATED(5.1, "GetFlags() is deprecated, please use GetDesc().Flags instead")
	ETextureCreateFlags GetFlags() const { return GetDesc().Flags; }

	//UE_DEPRECATED(5.1, "GetNumSamples() is deprecated, please use GetDesc().NumSamples instead")
	uint32 GetNumSamples() const { return GetDesc().NumSamples; }

	//UE_DEPRECATED(5.1, "GetClearBinding() is deprecated, please use GetDesc().ClearValue instead")
	const FClearValueBinding GetClearBinding() const { return GetDesc().ClearValue; }

	//UE_DEPRECATED(5.1, "GetSize() is deprecated, please use GetDesc().Extent.X instead")
	uint32 GetSize() const { check(GetDesc().IsTextureCube()); return GetDesc().Extent.X; }

#if ENABLE_RHI_VALIDATION
	virtual RHIValidation::FResource* GetValidationTrackerResource() override
	{
		// Use the method inherited from RHIValidation::FTextureResource, as that's already a virtual overridden
		// by subclasses such as FRHITextureReference to return the correct storage for the tracker information.
		return GetTrackerResource();
	}
#endif

private:

	friend class FRHITextureReference;
	/** Constructor for texture references */
	FRHITexture(ERHIResourceType InResourceType)
		: FRHIViewableResource(InResourceType, ERHIAccess::Unknown)
	{
		check(InResourceType == RRT_TextureReference);
	}

	FRHITextureDesc TextureDesc;

	FLastRenderTimeContainer LastRenderTime;
};

//
// Misc
//

class FRHITimestampCalibrationQuery : public FRHIResource
{
public:
	FRHITimestampCalibrationQuery() : FRHIResource(RRT_TimestampCalibrationQuery) {}
	uint64 GPUMicroseconds[MAX_NUM_GPUS] = {};
	uint64 CPUMicroseconds[MAX_NUM_GPUS] = {};
};

/*
* Generic GPU fence class.
* Granularity differs depending on backing RHI - ie it may only represent command buffer granularity.
* RHI specific fences derive from this to implement real GPU->CPU fencing.
* The default implementation always returns false for Poll until the next frame from the frame the fence was inserted
* because not all APIs have a GPU/CPU sync object, we need to fake it.
*/
class FRHIGPUFence : public FRHIResource
{
public:
	FRHIGPUFence(FName InName) : FRHIResource(RRT_GPUFence), FenceName(InName) {}
	virtual ~FRHIGPUFence() {}

	virtual void Clear() = 0;

	/**
	 * Poll the fence to see if the GPU has signaled it.
	 * @returns True if and only if the GPU fence has been inserted and the GPU has signaled the fence.
	 */
	virtual bool Poll() const = 0;

	/**
	 * Poll on a subset of the GPUs that this fence supports.
	 */
	virtual bool Poll(FRHIGPUMask GPUMask) const { return Poll(); }

	const FName& GetFName() const { return FenceName; }

	FThreadSafeCounter NumPendingWriteCommands;

protected:
	FName FenceName;
};

// Generic implementation of FRHIGPUFence
class FGenericRHIGPUFence : public FRHIGPUFence
{
public:
	RHI_API FGenericRHIGPUFence(FName InName);

	RHI_API virtual void Clear() final override;

	/** @discussion RHI implementations must be thread-safe and must correctly handle being called before RHIInsertFence if an RHI thread is active. */
	RHI_API virtual bool Poll() const final override;

	RHI_API void WriteInternal();

private:
	uint32 InsertedFrameNumber;
};

class FRHIRenderQuery : public FRHIResource
{
public:
	FRHIRenderQuery() : FRHIResource(RRT_RenderQuery) {}
};

class FRHIRenderQueryPool;

class FRHIPooledRenderQuery
{
	TRefCountPtr<FRHIRenderQuery> Query;
	FRHIRenderQueryPool* QueryPool = nullptr;

public:
	FRHIPooledRenderQuery() = default;
	FRHIPooledRenderQuery(FRHIRenderQueryPool* InQueryPool, TRefCountPtr<FRHIRenderQuery>&& InQuery);
	~FRHIPooledRenderQuery();

	FRHIPooledRenderQuery(const FRHIPooledRenderQuery&) = delete;
	FRHIPooledRenderQuery& operator=(const FRHIPooledRenderQuery&) = delete;
	FRHIPooledRenderQuery(FRHIPooledRenderQuery&&) = default;
	FRHIPooledRenderQuery& operator=(FRHIPooledRenderQuery&&) = default;

	bool IsValid() const
	{
		return Query.IsValid();
	}

	FRHIRenderQuery* GetQuery() const
	{
		return Query;
	}

	void ReleaseQuery();
};

class FRHIRenderQueryPool : public FRHIResource
{
public:
	FRHIRenderQueryPool() : FRHIResource(RRT_RenderQueryPool) {}
	virtual ~FRHIRenderQueryPool() {};
	virtual FRHIPooledRenderQuery AllocateQuery() = 0;

private:
	friend class FRHIPooledRenderQuery;
	virtual void ReleaseQuery(TRefCountPtr<FRHIRenderQuery>&& Query) = 0;
};

inline FRHIPooledRenderQuery::FRHIPooledRenderQuery(FRHIRenderQueryPool* InQueryPool, TRefCountPtr<FRHIRenderQuery>&& InQuery) 
	: Query(MoveTemp(InQuery))
	, QueryPool(InQueryPool)
{
	check(IsInParallelRenderingThread());
}

inline void FRHIPooledRenderQuery::ReleaseQuery()
{
	if (QueryPool && Query.IsValid())
	{
		QueryPool->ReleaseQuery(MoveTemp(Query));
		QueryPool = nullptr;
	}
	check(!Query.IsValid());
}

inline FRHIPooledRenderQuery::~FRHIPooledRenderQuery()
{
	check(IsInParallelRenderingThread());
	ReleaseQuery();
}

class FRHIViewport : public FRHIResource 
{
public:
	FRHIViewport() : FRHIResource(RRT_Viewport) {}

	/**
	 * Returns access to the platform-specific native resource pointer.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeSwapChain() const { return nullptr; }
	/**
	 * Returns access to the platform-specific native resource pointer to a backbuffer texture.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeBackBufferTexture() const { return nullptr; }
	/**
	 * Returns access to the platform-specific native resource pointer to a backbuffer rendertarget. This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeBackBufferRT() const { return nullptr; }

	/**
	 * Returns access to the platform-specific native window. This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all. 
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason.
	 * AddParam could represent any additional platform-specific data (could be null).
	 */
	virtual void* GetNativeWindow(void** AddParam = nullptr) const { return nullptr; }

	/**
	 * Sets custom Present handler on the viewport
	 */
	virtual void SetCustomPresent(class FRHICustomPresent*) {}

	/**
	 * Returns currently set custom present handler.
	 */
	virtual class FRHICustomPresent* GetCustomPresent() const { return nullptr; }


	/**
	 * Ticks the viewport on the Game thread
	 */
	virtual void Tick(float DeltaTime) {}

	virtual void WaitForFrameEventCompletion() { }

	virtual void IssueFrameEvent() { }
};

/** Used to specify a texture metadata plane when creating a view. */
enum class ERHITexturePlane : uint8
{
	// The primary plane is used with default compression behavior.
	Primary = 0,

	// The primary plane is used without decompressing it.
	PrimaryCompressed = 1,

	// The depth plane is used with default compression behavior.
	Depth = 2,

	// The stencil plane is used with default compression behavior.
	Stencil = 3,

	// The HTile plane is used.
	HTile = 4,

	// the FMask plane is used.
	FMask = 5,

	// the CMask plane is used.
	CMask = 6,

	// This enum is packed into various structures. Avoid adding new 
	// members without verifying structure sizes aren't increased.
	Num,
	NumBits = 3,

	// @todo deprecate
	None = Primary,
	CompressedSurface = PrimaryCompressed,
};
static_assert((1u << uint32(ERHITexturePlane::NumBits)) >= uint32(ERHITexturePlane::Num), "Not enough bits in the ERHITexturePlane enum");

//UE_DEPRECATED(5.3, "Use ERHITexturePlane.")
using ERHITextureMetaDataAccess = ERHITexturePlane;

//
// Views
//

template <typename TType>
struct TRHIRange
{
	TType First = 0;
	TType Num = 0;

	TRHIRange() = default;
	TRHIRange(uint32 InFirst, uint32 InNum)
		: First(InFirst)
		, Num  (InNum)
	{
		check( InFirst < TNumericLimits<TType>::Max()
			&& InNum   < TNumericLimits<TType>::Max()
			&& (InFirst + InNum) < TNumericLimits<TType>::Max());
	}

	TType ExclusiveLast() const { return First + Num; }
	TType InclusiveLast() const { return First + Num - 1; }

	bool IsInRange(uint32 Value) const
	{
		check(Value < TNumericLimits<TType>::Max());
		return Value >= First && Value < ExclusiveLast();
	}
};

using FRHIRange8  = TRHIRange<uint8>;
using FRHIRange16 = TRHIRange<uint16>;

//
// The unified RHI view descriptor. These are stored in the base FRHIView type, and packed to minimize memory usage.
// Platform RHI implementations use the GetViewInfo() functions to convert an FRHIViewDesc into the required info to make a view / descriptor for the GPU.
//
struct FRHIViewDesc
{
	enum class EViewType : uint8
	{
		BufferSRV,
		BufferUAV,
		TextureSRV,
		TextureUAV
	};

	enum class EBufferType : uint8
	{
		Unknown               = 0,

		Typed                 = 1,
		Structured            = 2,
		AccelerationStructure = 3,
		Raw                   = 4
	};

	enum class EDimension : uint8
	{
		Unknown          = 0,

		Texture2D        = 1,
		Texture2DArray   = 2,
		TextureCube      = 3,
		TextureCubeArray = 4,
		Texture3D        = 5,

		NumBits          = 3
	};

	// Properties that apply to all views.
	struct FCommon
	{
		EViewType    ViewType;
		EPixelFormat Format;
	};

	// Properties shared by buffer views. Some fields are SRV or UAV specific.
	struct FBuffer : public FCommon
	{
		EBufferType BufferType;
		uint8       bAtomicCounter : 1; // UAV only
		uint8       bAppendBuffer  : 1; // UAV only
		uint8       /* padding */  : 6;
		uint32      OffsetInBytes;
		uint32      NumElements;
		uint32      Stride;

		struct FViewInfo;
	protected:
		FViewInfo GetViewInfo(FRHIBuffer* TargetBuffer) const;
	};

	// Properties shared by texture views. Some fields are SRV or UAV specific.
	struct FTexture : public FCommon
	{
		ERHITexturePlane Plane        : uint32(ERHITexturePlane::NumBits);
		uint8            bDisableSRGB : 1; // SRV only
		EDimension       Dimension    : uint32(EDimension::NumBits);
		FRHIRange8       MipRange;    // UAVs only support 1 mip
		FRHIRange16      ArrayRange;

		struct FViewInfo;
	protected:
		FViewInfo GetViewInfo(FRHITexture* TargetTexture) const;
	};

	struct FBufferSRV : public FBuffer
	{
		struct FInitializer;
		struct FViewInfo;
		RHI_API FViewInfo GetViewInfo(FRHIBuffer* TargetBuffer) const;
	};

	struct FBufferUAV : public FBuffer
	{
		struct FInitializer;
		struct FViewInfo;
		RHI_API FViewInfo GetViewInfo(FRHIBuffer* TargetBuffer) const;
	};

	struct FTextureSRV : public FTexture
	{
		struct FInitializer;
		struct FViewInfo;
		RHI_API FViewInfo GetViewInfo(FRHITexture* TargetTexture) const;
	};

	struct FTextureUAV : public FTexture
	{
		struct FInitializer;
		struct FViewInfo;
		RHI_API FViewInfo GetViewInfo(FRHITexture* TargetTexture) const;
	};

	union
	{
		FCommon Common;
		union
		{
			FBufferSRV SRV;
			FBufferUAV UAV;
		} Buffer;
		union
		{
			FTextureSRV SRV;
			FTextureUAV UAV;
		} Texture;
	};

	static inline FBufferSRV::FInitializer CreateBufferSRV();
	static inline FBufferUAV::FInitializer CreateBufferUAV();

	static inline FTextureSRV::FInitializer CreateTextureSRV();
	static inline FTextureUAV::FInitializer CreateTextureUAV();

	bool IsSRV() const { return Common.ViewType == EViewType::BufferSRV || Common.ViewType == EViewType::TextureSRV; }
	bool IsUAV() const { return !IsSRV(); }

	bool IsBuffer () const { return Common.ViewType == EViewType::BufferSRV || Common.ViewType == EViewType::BufferUAV; }
	bool IsTexture() const { return !IsBuffer(); }

	bool operator == (FRHIViewDesc const& RHS) const
	{
		return FMemory::Memcmp(this, &RHS, sizeof(*this)) == 0;
	}

	bool operator != (FRHIViewDesc const& RHS) const
	{
		return !(*this == RHS);
	}

	FRHIViewDesc()
		: FRHIViewDesc(EViewType::BufferSRV)
	{
		FMemory::Memzero(*this);
	}

	static const TCHAR* GetBufferTypeString(EBufferType BufferType);
	static const TCHAR* GetTextureDimensionString(EDimension Dimension);

protected:
	FRHIViewDesc(EViewType ViewType)
	{
		FMemory::Memzero(*this);
		Common.ViewType = ViewType;
	}
};

// These static asserts are to ensure the descriptor is minimal in size and can be copied around by-value.
// If they fail, consider re-packing the struct.
static_assert(sizeof(FRHIViewDesc) == 16, "Packing of FRHIViewDesc is unexpected.");
static_assert(TIsTrivial<FRHIViewDesc>::Value, "FRHIViewDesc must be a trivial type.");

struct FRHIViewDesc::FBufferSRV::FInitializer : private FRHIViewDesc
{
	friend FRHIViewDesc;
	friend FRHICommandListBase;
	friend struct FShaderResourceViewInitializer;
	friend struct FRawBufferShaderResourceViewInitializer;

protected:
	FInitializer()
		: FRHIViewDesc(EViewType::BufferSRV)
	{}

public:
	FInitializer& SetType(EBufferType Type)
	{
		check(Type != EBufferType::Unknown);
		Buffer.SRV.BufferType = Type;
		return *this;
	}

	//
	// Provided for back-compat with existing code. Consider using SetType() instead for more direct control over the view.
	// For example, it is possible to create a typed view of a BUF_ByteAddress buffer, but not using this function which always choses raw access.
	//
	FInitializer& SetTypeFromBuffer(FRHIBuffer* TargetBuffer)
	{
		check(TargetBuffer);
		checkf(!TargetBuffer->GetDesc().IsNull(), TEXT("Null buffer resources are placeholders for the streaming system. They do not contain a valid descriptor for this function to use. Call SetType() instead."));

		Buffer.SRV.BufferType =
			EnumHasAnyFlags(TargetBuffer->GetUsage(), BUF_ByteAddressBuffer    ) ? EBufferType::Raw                   :
			EnumHasAnyFlags(TargetBuffer->GetUsage(), BUF_StructuredBuffer     ) ? EBufferType::Structured            :
			EnumHasAnyFlags(TargetBuffer->GetUsage(), BUF_AccelerationStructure) ? EBufferType::AccelerationStructure :
			EBufferType::Typed;
		return *this;
	}

	FInitializer& SetFormat(EPixelFormat InFormat)
	{
		Buffer.SRV.Format = InFormat;
		return *this;
	}

	FInitializer& SetOffsetInBytes(uint32 InOffsetBytes)
	{
		Buffer.SRV.OffsetInBytes = InOffsetBytes;
		return *this;
	}

	FInitializer& SetStride(uint32 InStride)
	{
		Buffer.SRV.Stride = InStride;
		return *this;
	}

	FInitializer& SetNumElements(uint32 InNumElements)
	{
		Buffer.SRV.NumElements = InNumElements;
		return *this;
	}
};

struct FRHIViewDesc::FBufferUAV::FInitializer : private FRHIViewDesc
{
	friend FRHIViewDesc;
	friend FRHICommandListBase;

protected:
	FInitializer()
		: FRHIViewDesc(EViewType::BufferUAV)
	{}

public:
	FInitializer& SetType(EBufferType Type)
	{
		check(Type != EBufferType::Unknown);
		Buffer.UAV.BufferType = Type;
		return *this;
	}

	//
	// Provided for back-compat with existing code. Consider using SetType() instead for more direct control over the view.
	// For example, it is possible to create a typed view of a BUF_ByteAddress buffer, but not using this function which always choses raw access.
	//
	FInitializer& SetTypeFromBuffer(FRHIBuffer* TargetBuffer)
	{
		check(TargetBuffer);
		checkf(!TargetBuffer->GetDesc().IsNull(), TEXT("Null buffer resources are placeholders for the streaming system. They do not contain a valid descriptor for this function to use. Call SetType() instead."));

		Buffer.UAV.BufferType =
			EnumHasAnyFlags(TargetBuffer->GetUsage(), BUF_ByteAddressBuffer    ) ? EBufferType::Raw                   :
			EnumHasAnyFlags(TargetBuffer->GetUsage(), BUF_StructuredBuffer     ) ? EBufferType::Structured            :
			EnumHasAnyFlags(TargetBuffer->GetUsage(), BUF_AccelerationStructure) ? EBufferType::AccelerationStructure :
			EBufferType::Typed;
		return *this;
	}

	FInitializer& SetFormat(EPixelFormat InFormat)
	{
		Buffer.UAV.Format = InFormat;
		return *this;
	}

	FInitializer& SetOffsetInBytes(uint32 InOffsetBytes)
	{
		Buffer.UAV.OffsetInBytes = InOffsetBytes;
		return *this;
	}

	FInitializer& SetStride(uint32 InStride)
	{
		Buffer.UAV.Stride = InStride;
		return *this;
	}

	FInitializer& SetNumElements(uint32 InNumElements)
	{
		Buffer.UAV.NumElements = InNumElements;
		return *this;
	}

	FInitializer& SetAtomicCounter(bool InAtomicCounter)
	{
		Buffer.UAV.bAtomicCounter = InAtomicCounter;
		return *this;
	}

	FInitializer& SetAppendBuffer(bool InAppendBuffer)
	{
		Buffer.UAV.bAppendBuffer = InAppendBuffer;
		return *this;
	}
};

struct FRHIViewDesc::FTextureSRV::FInitializer : private FRHIViewDesc
{
	friend FRHIViewDesc;
	friend FRHICommandListBase;

protected:
	FInitializer()
		: FRHIViewDesc(EViewType::TextureSRV)
	{}

public:
	//
	// Specifies the type of view to create. Must match the shader parameter this view will be bound to.
	// 
	// The dimension is allowed to differ from the underlying resource's dimensions, e.g. to create a view
	// compatible with a Texture2D<> shader parameter, but where the underlying resource is a texture 2D array.
	//
	// Some combinations are not valid, e.g. 3D textures can only have 3D views.
	//
	FInitializer& SetDimension(ETextureDimension InDimension)
	{
		switch (InDimension)
		{
		default: checkNoEntry(); break;
		case ETextureDimension::Texture2D       : Texture.SRV.Dimension = EDimension::Texture2D       ; break;
		case ETextureDimension::Texture2DArray  : Texture.SRV.Dimension = EDimension::Texture2DArray  ; break;
		case ETextureDimension::Texture3D       : Texture.SRV.Dimension = EDimension::Texture3D       ; break;
		case ETextureDimension::TextureCube     : Texture.SRV.Dimension = EDimension::TextureCube     ; break;
		case ETextureDimension::TextureCubeArray: Texture.SRV.Dimension = EDimension::TextureCubeArray; break;
		}
		return *this;
	}

	//
	// Provided for back-compat with existing code. Consider using SetDimension() instead for more direct control over the view.
	// For example, it is possible to create a 2D view of a 2DArray texture, but not using this function which always choses 2DArray dimension.
	//
	FInitializer& SetDimensionFromTexture(FRHITexture* TargetTexture)
	{
		check(TargetTexture);
		SetDimension(TargetTexture->GetDesc().Dimension);
		return *this;
	}

	FInitializer& SetFormat(EPixelFormat InFormat)
	{
		Texture.SRV.Format = InFormat;
		return *this;
	}

	FInitializer& SetPlane(ERHITexturePlane InPlane)
	{
		Texture.SRV.Plane = InPlane;
		return *this;
	}

	FInitializer& SetMipRange(uint8 InFirstMip, uint8 InNumMips)
	{
		Texture.SRV.MipRange.First = InFirstMip;
		Texture.SRV.MipRange.Num = InNumMips;
		return *this;
	}

	//
	// The meaning of array "elements" is given by the dimension of the underlying resource.
	// I.e. a view of a TextureCubeArray resource indexes the array in whole cubes.
	// 
	//     [0] = the first cube (2D slices 0 to 5)
	//     [1] = the second cube (2D slices 6 to 11)
	// 
	// If the view dimension is smaller than the resource dimension, the array range will be further limited.
	// E.g. creating a Texture2D dimension view of a TextureCubeArray resource
	//
	FInitializer& SetArrayRange(uint16 InFirstElement, uint16 InNumElements)
	{
		Texture.SRV.ArrayRange.First = InFirstElement;
		Texture.SRV.ArrayRange.Num = InNumElements;
		return *this;
	}

	FInitializer& SetDisableSRGB(bool InDisableSRGB)
	{
		Texture.SRV.bDisableSRGB = InDisableSRGB;
		return *this;
	}
};

struct FRHIViewDesc::FTextureUAV::FInitializer : private FRHIViewDesc
{
	friend FRHIViewDesc;
	friend FRHICommandListBase;

protected:
	FInitializer()
		: FRHIViewDesc(EViewType::TextureUAV)
	{
		// Texture UAVs only support 1 mip
		Texture.UAV.MipRange.Num = 1;
	}

public:
	//
	// Specifies the type of view to create. Must match the shader parameter this view will be bound to.
	// 
	// The dimension is allowed to differ from the underlying resource's dimensions, e.g. to create a view
	// compatible with an RWTexture2D<> shader parameter, but where the underlying resource is a texture 2D array.
	//
	// Some combinations are not valid, e.g. 3D textures can only have 3D views.
	//
	FInitializer& SetDimension(ETextureDimension InDimension)
	{
		switch (InDimension)
		{
		default: checkNoEntry(); break;
		case ETextureDimension::Texture2D       : Texture.UAV.Dimension = EDimension::Texture2D       ; break;
		case ETextureDimension::Texture2DArray  : Texture.UAV.Dimension = EDimension::Texture2DArray  ; break;
		case ETextureDimension::Texture3D       : Texture.UAV.Dimension = EDimension::Texture3D       ; break;
		case ETextureDimension::TextureCube     : Texture.UAV.Dimension = EDimension::TextureCube     ; break;
		case ETextureDimension::TextureCubeArray: Texture.UAV.Dimension = EDimension::TextureCubeArray; break;
		}
		return *this;
	}

	//
	// Provided for back-compat with existing code. Consider using SetDimension() instead for more direct control over the view.
	// For example, it is possible to create a 2D view of a 2DArray texture, but not using this function which always choses 2DArray dimension.
	//
	FInitializer& SetDimensionFromTexture(FRHITexture* TargetTexture)
	{
		check(TargetTexture);
		SetDimension(TargetTexture->GetDesc().Dimension);
		return *this;
	}

	FInitializer& SetFormat(EPixelFormat InFormat)
	{
		Texture.UAV.Format = InFormat;
		return *this;
	}

	FInitializer& SetPlane(ERHITexturePlane InPlane)
	{
		Texture.UAV.Plane = InPlane;
		return *this;
	}

	FInitializer& SetMipLevel(uint8 InMipLevel)
	{
		Texture.UAV.MipRange.First = InMipLevel;
		return *this;
	}

	//
	// The meaning of array "elements" is given by the dimension of the underlying resource.
	// I.e. a view of a TextureCubeArray resource indexes the array in whole cubes.
	// 
	//     [0] = the first cube (2D slices 0 to 5)
	//     [1] = the second cube (2D slices 6 to 11)
	// 
	// If the view dimension is smaller than the resource dimension, the array range will be further limited.
	// E.g. creating a Texture2D dimension view of a TextureCubeArray resource
	//
	FInitializer& SetArrayRange(uint16 InFirstElement, uint16 InNumElements)
	{
		Texture.UAV.ArrayRange.First = InFirstElement;
		Texture.UAV.ArrayRange.Num = InNumElements;
		return *this;
	}
};

inline FRHIViewDesc::FBufferSRV::FInitializer FRHIViewDesc::CreateBufferSRV()
{
	return FRHIViewDesc::FBufferSRV::FInitializer();
}

inline FRHIViewDesc::FBufferUAV::FInitializer FRHIViewDesc::CreateBufferUAV()
{
	return FRHIViewDesc::FBufferUAV::FInitializer();
}

inline FRHIViewDesc::FTextureSRV::FInitializer FRHIViewDesc::CreateTextureSRV()
{
	return FRHIViewDesc::FTextureSRV::FInitializer();
}

inline FRHIViewDesc::FTextureUAV::FInitializer FRHIViewDesc::CreateTextureUAV()
{
	return FRHIViewDesc::FTextureUAV::FInitializer();
}

//
// Used by platform RHIs to create views of buffers. The data in this structure is computed in GetViewInfo(),
// and is specific to a particular buffer resource. It is not intended to be stored in a view instance.
//
struct FRHIViewDesc::FBuffer::FViewInfo
{
	// The offset in bytes from the beginning of the viewed buffer resource.
	uint32 OffsetInBytes;

	// The size in bytes of a single element in the view.
	uint32 StrideInBytes;

	// The number of elements visible in the view.
	uint32 NumElements;

	// The total number of bytes the data visible in the view covers (i.e. stride * numelements).
	uint32 SizeInBytes;

	// Whether this is a typed / structured / raw view etc.
	EBufferType BufferType;

	// The format of the data exposed by this view. PF_Unknown for all buffer types except typed buffer views.
	EPixelFormat Format;

	// When true, the view is refering to a BUF_NullResource, so a null descriptor should be created.
	bool bNullView;
};

// Buffer SRV specific info
struct FRHIViewDesc::FBufferSRV::FViewInfo : public FRHIViewDesc::FBuffer::FViewInfo
{};

// Buffer UAV specific info
struct FRHIViewDesc::FBufferUAV::FViewInfo : public FRHIViewDesc::FBuffer::FViewInfo
{
	bool bAtomicCounter = false;
	bool bAppendBuffer = false;
};

//
// Used by platform RHIs to create views of textures. The data in this structure is computed in GetViewInfo(),
// and is specific to a particular texture resource. It is not intended to be stored in a view instance.
//
struct FRHIViewDesc::FTexture::FViewInfo
{
	//
	// The range of array "elements" the view covers.
	// 
	// The meaning of "elements" is given by the view dimension.
	// I.e. a view with "Dimension == CubeArray" indexes the array in whole cubes.
	// 
	//		- [0]: the first cube (2D slices 0 to 5)
	//		- [1]: the second cube (2D slices 6 to 11)
	// 
	// 3D textures always have ArrayRange.Num == 1 because there are no "3D texture arrays".
	//
	FRHIRange16 ArrayRange;

	// Which plane of a texture to access (i.e. color, depth, stencil etc). 
	ERHITexturePlane Plane;

	// The typed format to use when reading / writing data in the viewed texture.
	EPixelFormat Format;

	// Specifies how to treat the texture resource when creating the view.
	// E.g. it is possible to create a 2DArray view of a 2D or Cube texture.
	EDimension Dimension : uint32(EDimension::NumBits);

	// True when the view covers every mip of the resource.
	uint8 bAllMips : 1;

	// True when the view covers every array slice of the resource.
	// This includes depth slices for 3D textures, and faces of texture cubes.
	uint8 bAllSlices : 1;
};

// Texture SRV specific info
struct FRHIViewDesc::FTextureSRV::FViewInfo : public FRHIViewDesc::FTexture::FViewInfo
{
	// The range of texture mips the view covers.
	FRHIRange8 MipRange;

	// Indicates if this view should use an sRGB variant of the typed format.
	uint8 bSRGB : 1;
};


// Texture UAV specific info
struct FRHIViewDesc::FTextureUAV::FViewInfo : public FRHIViewDesc::FTexture::FViewInfo
{
	// The single mip level covered by this view.
	uint8 MipLevel;
};

class FRHIView : public FRHIResource
{
public:
	FRHIView(ERHIResourceType InResourceType, FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc)
		: FRHIResource(InResourceType)
		, Resource(InResource)
		, ViewDesc(InViewDesc)
	{
		checkf(InResource, TEXT("Cannot create a view of a nullptr resource."));
	}

	virtual FRHIDescriptorHandle GetBindlessHandle() const
	{
		return FRHIDescriptorHandle();
	}

	FRHIViewableResource* GetResource() const
	{
		return Resource;
	}

	FRHIBuffer* GetBuffer() const
	{
		check(ViewDesc.IsBuffer());
		return static_cast<FRHIBuffer*>(Resource.GetReference());
	}

	FRHITexture* GetTexture() const
	{
		check(ViewDesc.IsTexture());
		return static_cast<FRHITexture*>(Resource.GetReference());
	}

	bool IsBuffer () const { return ViewDesc.IsBuffer (); }
	bool IsTexture() const { return ViewDesc.IsTexture(); }

#if ENABLE_RHI_VALIDATION
	RHIValidation::FViewIdentity GetViewIdentity() const
	{
		return RHIValidation::FViewIdentity(Resource, ViewDesc);
	}
#endif

	FRHIViewDesc const& GetDesc() const
	{
		return ViewDesc;
	}

private:
	TRefCountPtr<FRHIViewableResource> Resource;

protected:
	FRHIViewDesc const ViewDesc;
};

class FRHIUnorderedAccessView : public FRHIView
{
public:
	explicit FRHIUnorderedAccessView(FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc)
		: FRHIView(RRT_UnorderedAccessView, InResource, InViewDesc)
	{
		check(ViewDesc.IsUAV());
	}
};

class FRHIShaderResourceView : public FRHIView
{
public:
	explicit FRHIShaderResourceView(FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc)
		: FRHIView(RRT_ShaderResourceView, InResource, InViewDesc)
	{
		check(ViewDesc.IsSRV());
	}
};

/**
 * A type used only for printing a string for debugging/profiling.
 * Adds Number as a suffix to the printed string even if the base name includes a number, so may prints a string like: Base_1_1
 * This type will always store a numeric suffix explicitly inside itself and never in the name table so it will always be at least 12 bytes
 * regardless of the value of UE_FNAME_OUTLINE_NUMBER.
 * It is not comparable or convertible to other name types to encourage its use only for debugging and avoid using more storage than necessary
 * for the primary use cases of FName (names of objects, assets etc which are widely used and therefor deduped in the name table).
 */
class FDebugName
{
public:
	RHI_API FDebugName();
	RHI_API FDebugName(FName InName);
	RHI_API FDebugName(FName InName, int32 InNumber);

	RHI_API FDebugName& operator=(FName Other);

	RHI_API FString ToString() const;
	bool IsNone() const { return Name.IsNone() && Number == NAME_NO_NUMBER_INTERNAL; }
	RHI_API void AppendString(FStringBuilderBase& Builder) const;

private:
	FName Name;
	uint32 Number;
};

//
// Ray tracing resources
//

enum class ERayTracingInstanceFlags : uint8
{
	None = 0,
	TriangleCullDisable = 1 << 1, // No back face culling. Triangle is visible from both sides.
	TriangleCullReverse = 1 << 2, // Makes triangle front-facing if its vertices are counterclockwise from ray origin.
	ForceOpaque = 1 << 3, // Disable any-hit shader invocation for this instance.
	ForceNonOpaque = 1 << 4, // Force any-hit shader invocation even if geometries inside the instance were marked opaque.
};
ENUM_CLASS_FLAGS(ERayTracingInstanceFlags);

class FRHIRayTracingGeometry;
/**
* High level descriptor of one or more instances of a mesh in a ray tracing scene.
* All instances covered by this descriptor will share shader bindings, but may have different transforms and user data.
*/
struct FRayTracingGeometryInstance
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FRayTracingGeometryInstance() = default;
	FRayTracingGeometryInstance(const FRayTracingGeometryInstance&) = default;
	FRayTracingGeometryInstance& operator=(const FRayTracingGeometryInstance&) = default;
	FRayTracingGeometryInstance(FRayTracingGeometryInstance&&) = default;
	FRayTracingGeometryInstance& operator=(FRayTracingGeometryInstance&&) = default;
	~FRayTracingGeometryInstance() = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FRHIRayTracingGeometry* GeometryRHI = nullptr;

	// A single physical mesh may be duplicated many times in the scene with different transforms and user data.
	// All copies share the same shader binding table entries and therefore will have the same material and shader resources.
	TArrayView<const FMatrix> Transforms;

	// Offsets into the scene's instance scene data buffer used to get instance transforms from GPUScene
	// If BaseInstanceSceneDataOffset != -1, instances are assumed to be continuous.
	int32 BaseInstanceSceneDataOffset = -1;
	TArrayView<const uint32> InstanceSceneDataOffsets;

	// Optional buffer that stores GPU transforms. Used instead of CPU-side transform data.
	FShaderResourceViewRHIRef GPUTransformsSRV = nullptr;

	// Conservative number of instances. Some of the actual instances may be made inactive if GPU transforms are used.
	// Must be less or equal to number of entries in Transforms view if CPU transform data is used.
	// Must be less or equal to number of entries in GPUTransformsSRV if it is non-null.
	uint32 NumTransforms = 0;

	// Each geometry copy can receive a user-provided integer, which can be used to retrieve extra shader parameters or customize appearance.
	// This data can be retrieved using GetInstanceUserData() in closest/any hit shaders.
	// If UserData view is empty, then DefaultUserData value will be used for all instances.
	// If UserData view is used, then it must have the same number of entries as NumInstances.
	uint32 DefaultUserData = 0;
	TArrayView<const uint32> UserData;

	// Each geometry copy can have one bit to make it individually deactivated (removed from TLAS while maintaining hit group indexing). Useful for culling.
	UE_DEPRECATED(5.4, "ActivationMask has been deprecated.")
	TArrayView<const uint32> ActivationMask;

	// Whether local bounds scale and center translation should be applied to the instance transform.
	bool bApplyLocalBoundsTransform = false;

	// Mask that will be tested against one provided to TraceRay() in shader code.
	// If binary AND of instance mask with ray mask is zero, then the instance is considered not intersected / invisible.
	uint8 Mask = 0xFF;

	uint8 LayerIndex = 0;

	// Flags to control triangle back face culling, whether to allow any-hit shaders, etc.
	ERayTracingInstanceFlags Flags = ERayTracingInstanceFlags::None;
};

enum ERayTracingGeometryType
{
	// Indexed or non-indexed triangle list with fixed function ray intersection.
	// Vertex buffer must contain vertex positions as VET_Float3.
	// Vertex stride must be at least 12 bytes, but may be larger to support custom per-vertex data.
	// Index buffer may be provided for indexed triangle lists. Implicit triangle list is assumed otherwise.
	RTGT_Triangles,

	// Custom primitive type that requires an intersection shader.
	// Vertex buffer for procedural geometry must contain one AABB per primitive as {float3 MinXYZ, float3 MaxXYZ}.
	// Vertex stride must be at least 24 bytes, but may be larger to support custom per-primitive data.
	// Index buffers can't be used with procedural geometry.
	RTGT_Procedural,
};
DECLARE_INTRINSIC_TYPE_LAYOUT(ERayTracingGeometryType);

enum class ERayTracingGeometryInitializerType
{
	// Fully initializes the RayTracingGeometry object: creates underlying buffer and initializes shader parameters.
	Rendering,

	// Does not create underlying buffer or shader parameters. Used by the streaming system as an object that is streamed into. 
	StreamingDestination,

	// Creates buffers but does not create shader parameters. Used for intermediate objects in the streaming system.
	StreamingSource,
};
DECLARE_INTRINSIC_TYPE_LAYOUT(ERayTracingGeometryInitializerType);

struct FRayTracingGeometrySegment
{
public:
	FBufferRHIRef VertexBuffer = nullptr;
	EVertexElementType VertexBufferElementType = VET_Float3;

	// Offset in bytes from the base address of the vertex buffer.
	uint32 VertexBufferOffset = 0;

	// Number of bytes between elements of the vertex buffer (sizeof VET_Float3 by default).
	// Must be equal or greater than the size of the position vector.
	uint32 VertexBufferStride = 12;

	// Number of vertices (positions) in VertexBuffer.
	// If an index buffer is present, this must be at least the maximum index value in the index buffer + 1.
	uint32 MaxVertices = 0;

	// Primitive range for this segment.
	uint32 FirstPrimitive = 0;
	uint32 NumPrimitives = 0;

	// Indicates whether any-hit shader could be invoked when hitting this geometry segment.
	// Setting this to `false` turns off any-hit shaders, making the section "opaque" and improving ray tracing performance.
	bool bForceOpaque = false;

	// Any-hit shader may be invoked multiple times for the same primitive during ray traversal.
	// Setting this to `false` guarantees that only a single instance of any-hit shader will run per primitive, at some performance cost.
	bool bAllowDuplicateAnyHitShaderInvocation = true;

	// Indicates whether this section is enabled and should be taken into account during acceleration structure creation
	bool bEnabled = true;
};

struct FRayTracingGeometryInitializer
{
public:
	FBufferRHIRef IndexBuffer = nullptr;

	// Offset in bytes from the base address of the index buffer.
	uint32 IndexBufferOffset = 0;

	ERayTracingGeometryType GeometryType = RTGT_Triangles;

	// Total number of primitives in all segments of the geometry. Only used for validation.
	uint32 TotalPrimitiveCount = 0;

	// Partitions of geometry to allow different shader and resource bindings.
	// All ray tracing geometries must have at least one segment.
	TArray<FRayTracingGeometrySegment> Segments;

	// Offline built geometry data. If null, the geometry will be built by the RHI at runtime.
	FResourceArrayInterface* OfflineData = nullptr;

	// Pointer to an existing ray tracing geometry which the new geometry is built from.
	FRHIRayTracingGeometry* SourceGeometry = nullptr;

	bool bFastBuild = false;
	bool bAllowUpdate = false;
	bool bAllowCompaction = true;
	ERayTracingGeometryInitializerType Type = ERayTracingGeometryInitializerType::Rendering;

	// Use FDebugName for auto-generated debug names with numbered suffixes, it is a variation of FMemoryImageName with optional number postfix.
	FDebugName DebugName;
	// Store the path name of the owner object for resource tracking. FMemoryImageName allows a conversion to/from FName.
	FName OwnerName;
};

enum ERayTracingSceneLifetime
{
	// Scene may only be used during the frame when it was created.
	RTSL_SingleFrame,

	// Scene may be constructed once and used in any number of later frames (not currently implemented).
	// RTSL_MultiFrame,
};

enum class ERayTracingAccelerationStructureFlags
{
	None = 0,
	AllowUpdate = 1 << 0,
	AllowCompaction = 1 << 1,
	FastTrace = 1 << 2,
	FastBuild = 1 << 3,
	MinimizeMemory = 1 << 4,
};
ENUM_CLASS_FLAGS(ERayTracingAccelerationStructureFlags);

struct FRayTracingSceneInitializer2
{
	// Unique list of geometries referenced by all instances in this scene.
	// Any referenced geometry is kept alive while the scene is alive.
	TArray<TRefCountPtr<FRHIRayTracingGeometry>> ReferencedGeometries;
	// One entry per instance
	TArray<FRHIRayTracingGeometry*> PerInstanceGeometries;
	// Exclusive prefix sum of `Instance.NumTransforms` for all instances in this scene. Used to emulate SV_InstanceID in hit shaders.
	TArray<uint32> BaseInstancePrefixSum;
	// Exclusive prefix sum of instance geometry segments is used to calculate SBT record address from instance and segment indices.
	TArray<uint32> SegmentPrefixSum;

	// Total flattened number of ray tracing geometry instances (a single FRayTracingGeometryInstance may represent many) per layer.
	TArray<uint32> NumNativeInstancesPerLayer;

	UE_DEPRECATED(5.1, "Use NumNativeInstancesPerLayer instead.")
	uint32 NumNativeInstances = 0;

	uint32 NumTotalSegments = 0;

	// This value controls how many elements will be allocated in the shader binding table per geometry segment.
	// Changing this value allows different hit shaders to be used for different effects.
	// For example, setting this to 2 allows one hit shader for regular material evaluation and a different one for shadows.
	// Desired hit shader can be selected by providing appropriate RayContributionToHitGroupIndex to TraceRay() function.
	// Use ShaderSlot argument in SetRayTracingHitGroup() to assign shaders and resources for specific part of the shder binding table record.
	uint32 ShaderSlotsPerGeometrySegment = 1;

	// Defines how many different callable shaders with unique resource bindings can be bound to this scene.
	// Shaders and resources are assigned to slots in the scene using SetRayTracingCallableShader().
	uint32 NumCallableShaderSlots = 0;

	// At least one miss shader must be present in a ray tracing scene.
	// Default miss shader is always in slot 0. Default shader must not use local resources.
	// Custom miss shaders can be bound to other slots using SetRayTracingMissShader().
	uint32 NumMissShaderSlots = 1;

	// Defines whether data in this scene should persist between frames.
	// Currently only single-frame lifetime is supported.
	ERayTracingSceneLifetime Lifetime = RTSL_SingleFrame;

	FName DebugName;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FRayTracingSceneInitializer2() = default;
	FRayTracingSceneInitializer2(FRayTracingSceneInitializer2&&) = default;
	FRayTracingSceneInitializer2& operator=(FRayTracingSceneInitializer2&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

struct FRayTracingAccelerationStructureSize
{
	uint64 ResultSize = 0;
	uint64 BuildScratchSize = 0;
	uint64 UpdateScratchSize = 0;
};

class FRHIRayTracingAccelerationStructure
	: public FRHIResource
#if ENABLE_RHI_VALIDATION
	, public RHIValidation::FAccelerationStructureResource
#endif
{
public:
	FRHIRayTracingAccelerationStructure() : FRHIResource(RRT_RayTracingAccelerationStructure) {}

	FRayTracingAccelerationStructureSize GetSizeInfo() const
	{
		return SizeInfo;
	};

protected:
	FRayTracingAccelerationStructureSize SizeInfo = {};
};

using FRayTracingAccelerationStructureAddress = uint64;

/** Bottom level ray tracing acceleration structure (contains triangles). */
class FRHIRayTracingGeometry : public FRHIRayTracingAccelerationStructure
{
public:
	FRHIRayTracingGeometry() = default;
	FRHIRayTracingGeometry(const FRayTracingGeometryInitializer& InInitializer)
		: Initializer(InInitializer)
		, InitializedType(InInitializer.Type)
	{}

	virtual FRayTracingAccelerationStructureAddress GetAccelerationStructureAddress(uint64 GPUIndex) const = 0;
	virtual void SetInitializer(const FRayTracingGeometryInitializer& Initializer) = 0;
	virtual bool IsCompressed() const { return false; }

	const FRayTracingGeometryInitializer& GetInitializer() const
	{
		return Initializer;
	}

	uint32 GetNumSegments() const 
	{ 
		return Initializer.Segments.Num(); 
	}
protected:
	FRayTracingGeometryInitializer Initializer = {};
	ERayTracingGeometryInitializerType InitializedType = ERayTracingGeometryInitializerType::Rendering;
};

/** Top level ray tracing acceleration structure (contains instances of meshes). */
class FRHIRayTracingScene : public FRHIRayTracingAccelerationStructure
{
public:
	virtual const FRayTracingSceneInitializer2& GetInitializer() const = 0;

	// Returns a buffer view for RHI-specific system parameters associated with this scene.
	// This may be needed to access ray tracing geometry data in shaders that use ray queries.
	// Returns NULL if current RHI does not require this buffer.
	virtual FRHIShaderResourceView* GetOrCreateMetadataBufferSRV(FRHICommandListImmediate& RHICmdList)
	{
		return nullptr;
	}

	virtual uint32 GetLayerBufferOffset(uint32 LayerIndex) const = 0;
};

class FRHIShaderBundle : public FRHIResource
{
public:
	// Dispatch XYZ + Padding
	static constexpr uint32 ArgumentByteStride = sizeof(uint32) * 4u;

	const uint32 NumRecords = 0;

public:
	FRHIShaderBundle(uint32 InNumRecords)
		: FRHIResource(RRT_ShaderBundle)
		, NumRecords(InNumRecords)
	{
	}
};

/* Generic staging buffer class used by FRHIGPUMemoryReadback
* RHI specific staging buffers derive from this
*/
class FRHIStagingBuffer : public FRHIResource
{
public:
	FRHIStagingBuffer()
		: FRHIResource(RRT_StagingBuffer)
		, bIsLocked(false)
	{}

	virtual ~FRHIStagingBuffer() {}

	virtual void *Lock(uint32 Offset, uint32 NumBytes) = 0;
	virtual void Unlock() = 0;

	// For debugging, may not be implemented on all RHIs
	virtual uint64 GetGPUSizeBytes() const { return 0; }

protected:
	bool bIsLocked;
};

class FGenericRHIStagingBuffer : public FRHIStagingBuffer
{
public:
	FGenericRHIStagingBuffer()
		: FRHIStagingBuffer()
	{}

	~FGenericRHIStagingBuffer() {}

	RHI_API virtual void* Lock(uint32 Offset, uint32 NumBytes) final override;
	RHI_API virtual void Unlock() final override;
	virtual uint64 GetGPUSizeBytes() const final override { return ShadowBuffer.IsValid() ? ShadowBuffer->GetSize() : 0; }

	FBufferRHIRef ShadowBuffer;
	uint32 Offset;
};

class FRHIRenderTargetView
{
public:
	FRHITexture* Texture = nullptr;
	uint32 MipIndex = 0;

	/** Array slice or texture cube face.  Only valid if texture resource was created with TexCreate_TargetArraySlicesIndependently! */
	uint32 ArraySliceIndex = -1;

	ERenderTargetLoadAction LoadAction = ERenderTargetLoadAction::ENoAction;
	ERenderTargetStoreAction StoreAction = ERenderTargetStoreAction::ENoAction;

	FRHIRenderTargetView() = default;
	FRHIRenderTargetView(FRHIRenderTargetView&&) = default;
	FRHIRenderTargetView(const FRHIRenderTargetView&) = default;
	FRHIRenderTargetView& operator=(FRHIRenderTargetView&&) = default;
	FRHIRenderTargetView& operator=(const FRHIRenderTargetView&) = default;

	//common case
	explicit FRHIRenderTargetView(FRHITexture* InTexture, ERenderTargetLoadAction InLoadAction) :
		Texture(InTexture),
		MipIndex(0),
		ArraySliceIndex(-1),
		LoadAction(InLoadAction),
		StoreAction(ERenderTargetStoreAction::EStore)
	{}

	//common case
	explicit FRHIRenderTargetView(FRHITexture* InTexture, ERenderTargetLoadAction InLoadAction, uint32 InMipIndex, uint32 InArraySliceIndex) :
		Texture(InTexture),
		MipIndex(InMipIndex),
		ArraySliceIndex(InArraySliceIndex),
		LoadAction(InLoadAction),
		StoreAction(ERenderTargetStoreAction::EStore)
	{}
	
	explicit FRHIRenderTargetView(FRHITexture* InTexture, uint32 InMipIndex, uint32 InArraySliceIndex, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction) :
		Texture(InTexture),
		MipIndex(InMipIndex),
		ArraySliceIndex(InArraySliceIndex),
		LoadAction(InLoadAction),
		StoreAction(InStoreAction)
	{}

	bool operator==(const FRHIRenderTargetView& Other) const
	{
		return 
			Texture == Other.Texture &&
			MipIndex == Other.MipIndex &&
			ArraySliceIndex == Other.ArraySliceIndex &&
			LoadAction == Other.LoadAction &&
			StoreAction == Other.StoreAction;
	}
};

class FRHIDepthRenderTargetView
{
public:
	FRHITexture* Texture;

	ERenderTargetLoadAction		DepthLoadAction;
	ERenderTargetStoreAction	DepthStoreAction;
	ERenderTargetLoadAction		StencilLoadAction;

private:
	ERenderTargetStoreAction	StencilStoreAction;
	FExclusiveDepthStencil		DepthStencilAccess;
public:

	// accessor to prevent write access to StencilStoreAction
	ERenderTargetStoreAction GetStencilStoreAction() const { return StencilStoreAction; }
	// accessor to prevent write access to DepthStencilAccess
	FExclusiveDepthStencil GetDepthStencilAccess() const { return DepthStencilAccess; }

	explicit FRHIDepthRenderTargetView() :
		Texture(nullptr),
		DepthLoadAction(ERenderTargetLoadAction::ENoAction),
		DepthStoreAction(ERenderTargetStoreAction::ENoAction),
		StencilLoadAction(ERenderTargetLoadAction::ENoAction),
		StencilStoreAction(ERenderTargetStoreAction::ENoAction),
		DepthStencilAccess(FExclusiveDepthStencil::DepthNop_StencilNop)
	{
		Validate();
	}

	//common case
	explicit FRHIDepthRenderTargetView(FRHITexture* InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction) :
		Texture(InTexture),
		DepthLoadAction(InLoadAction),
		DepthStoreAction(InStoreAction),
		StencilLoadAction(InLoadAction),
		StencilStoreAction(InStoreAction),
		DepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FRHITexture* InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction, FExclusiveDepthStencil InDepthStencilAccess) :
		Texture(InTexture),
		DepthLoadAction(InLoadAction),
		DepthStoreAction(InStoreAction),
		StencilLoadAction(InLoadAction),
		StencilStoreAction(InStoreAction),
		DepthStencilAccess(InDepthStencilAccess)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FRHITexture* InTexture, ERenderTargetLoadAction InDepthLoadAction, ERenderTargetStoreAction InDepthStoreAction, ERenderTargetLoadAction InStencilLoadAction, ERenderTargetStoreAction InStencilStoreAction) :
		Texture(InTexture),
		DepthLoadAction(InDepthLoadAction),
		DepthStoreAction(InDepthStoreAction),
		StencilLoadAction(InStencilLoadAction),
		StencilStoreAction(InStencilStoreAction),
		DepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FRHITexture* InTexture, ERenderTargetLoadAction InDepthLoadAction, ERenderTargetStoreAction InDepthStoreAction, ERenderTargetLoadAction InStencilLoadAction, ERenderTargetStoreAction InStencilStoreAction, FExclusiveDepthStencil InDepthStencilAccess) :
		Texture(InTexture),
		DepthLoadAction(InDepthLoadAction),
		DepthStoreAction(InDepthStoreAction),
		StencilLoadAction(InStencilLoadAction),
		StencilStoreAction(InStencilStoreAction),
		DepthStencilAccess(InDepthStencilAccess)
	{
		Validate();
	}

	void Validate() const
	{
		// VK and Metal MAY leave the attachment in an undefined state if the StoreAction is DontCare. So we can't assume read-only implies it should be DontCare unless we know for sure it will never be used again.
		// ensureMsgf(DepthStencilAccess.IsDepthWrite() || DepthStoreAction == ERenderTargetStoreAction::ENoAction, TEXT("Depth is read-only, but we are performing a store.  This is a waste on mobile.  If depth can't change, we don't need to store it out again"));
		/*ensureMsgf(DepthStencilAccess.IsStencilWrite() || StencilStoreAction == ERenderTargetStoreAction::ENoAction, TEXT("Stencil is read-only, but we are performing a store.  This is a waste on mobile.  If stencil can't change, we don't need to store it out again"));*/
	}

	bool operator==(const FRHIDepthRenderTargetView& Other) const
	{
		return
			Texture == Other.Texture &&
			DepthLoadAction == Other.DepthLoadAction &&
			DepthStoreAction == Other.DepthStoreAction &&
			StencilLoadAction == Other.StencilLoadAction &&
			StencilStoreAction == Other.StencilStoreAction &&
			DepthStencilAccess == Other.DepthStencilAccess;
	}
};

class FRHISetRenderTargetsInfo
{
public:
	// Color Render Targets Info
	FRHIRenderTargetView ColorRenderTarget[MaxSimultaneousRenderTargets];	
	int32 NumColorRenderTargets;
	bool bClearColor;

	// Color Render Targets Info
	FRHIRenderTargetView ColorResolveRenderTarget[MaxSimultaneousRenderTargets];	
	bool bHasResolveAttachments;

	// Depth/Stencil Render Target Info
	FRHIDepthRenderTargetView DepthStencilRenderTarget;	
	bool bClearDepth;
	bool bClearStencil;

	FRHITexture* ShadingRateTexture;
	EVRSRateCombiner ShadingRateTextureCombiner;

	uint8 MultiViewCount;

	FRHISetRenderTargetsInfo() :
		NumColorRenderTargets(0),
		bClearColor(false),
		bHasResolveAttachments(false),
		bClearDepth(false),
		ShadingRateTexture(nullptr),
		MultiViewCount(0)
	{}

	FRHISetRenderTargetsInfo(int32 InNumColorRenderTargets, const FRHIRenderTargetView* InColorRenderTargets, const FRHIDepthRenderTargetView& InDepthStencilRenderTarget) :
		NumColorRenderTargets(InNumColorRenderTargets),
		bClearColor(InNumColorRenderTargets > 0 && InColorRenderTargets[0].LoadAction == ERenderTargetLoadAction::EClear),
		bHasResolveAttachments(false),
		DepthStencilRenderTarget(InDepthStencilRenderTarget),		
		bClearDepth(InDepthStencilRenderTarget.Texture && InDepthStencilRenderTarget.DepthLoadAction == ERenderTargetLoadAction::EClear),
		ShadingRateTexture(nullptr),
		ShadingRateTextureCombiner(VRSRB_Passthrough)
	{
		check(InNumColorRenderTargets <= 0 || InColorRenderTargets);
		for (int32 Index = 0; Index < InNumColorRenderTargets; ++Index)
		{
			ColorRenderTarget[Index] = InColorRenderTargets[Index];			
		}
	}
	// @todo metal mrt: This can go away after all the cleanup is done
	void SetClearDepthStencil(bool bInClearDepth, bool bInClearStencil = false)
	{
		if (bInClearDepth)
		{
			DepthStencilRenderTarget.DepthLoadAction = ERenderTargetLoadAction::EClear;
		}
		if (bInClearStencil)
		{
			DepthStencilRenderTarget.StencilLoadAction = ERenderTargetLoadAction::EClear;
		}
		bClearDepth = bInClearDepth;		
		bClearStencil = bInClearStencil;		
	}

	uint32 CalculateHash() const
	{
		// Need a separate struct so we can memzero/remove dependencies on reference counts
		struct FHashableStruct
		{
			// *2 for color and resolves, depth goes in the second-to-last slot, shading rate goes in the last slot
			FRHITexture* Texture[MaxSimultaneousRenderTargets*2 + 2];
			uint32 MipIndex[MaxSimultaneousRenderTargets];
			uint32 ArraySliceIndex[MaxSimultaneousRenderTargets];
			ERenderTargetLoadAction LoadAction[MaxSimultaneousRenderTargets];
			ERenderTargetStoreAction StoreAction[MaxSimultaneousRenderTargets];

			ERenderTargetLoadAction		DepthLoadAction;
			ERenderTargetStoreAction	DepthStoreAction;
			ERenderTargetLoadAction		StencilLoadAction;
			ERenderTargetStoreAction	StencilStoreAction;
			FExclusiveDepthStencil		DepthStencilAccess;

			bool bClearDepth;
			bool bClearStencil;
			bool bClearColor;
			bool bHasResolveAttachments;
			FRHIUnorderedAccessView* UnorderedAccessView[MaxSimultaneousUAVs];
			uint8 MultiViewCount;

			void Set(const FRHISetRenderTargetsInfo& RTInfo)
			{
				FMemory::Memzero(*this);
				for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
				{
					Texture[Index] = RTInfo.ColorRenderTarget[Index].Texture;
					Texture[MaxSimultaneousRenderTargets+Index] = RTInfo.ColorResolveRenderTarget[Index].Texture;
					MipIndex[Index] = RTInfo.ColorRenderTarget[Index].MipIndex;
					ArraySliceIndex[Index] = RTInfo.ColorRenderTarget[Index].ArraySliceIndex;
					LoadAction[Index] = RTInfo.ColorRenderTarget[Index].LoadAction;
					StoreAction[Index] = RTInfo.ColorRenderTarget[Index].StoreAction;
				}

				Texture[MaxSimultaneousRenderTargets] = RTInfo.DepthStencilRenderTarget.Texture;
				Texture[MaxSimultaneousRenderTargets + 1] = RTInfo.ShadingRateTexture;
				DepthLoadAction = RTInfo.DepthStencilRenderTarget.DepthLoadAction;
				DepthStoreAction = RTInfo.DepthStencilRenderTarget.DepthStoreAction;
				StencilLoadAction = RTInfo.DepthStencilRenderTarget.StencilLoadAction;
				StencilStoreAction = RTInfo.DepthStencilRenderTarget.GetStencilStoreAction();
				DepthStencilAccess = RTInfo.DepthStencilRenderTarget.GetDepthStencilAccess();

				bClearDepth = RTInfo.bClearDepth;
				bClearStencil = RTInfo.bClearStencil;
				bClearColor = RTInfo.bClearColor;
				bHasResolveAttachments = RTInfo.bHasResolveAttachments;
				MultiViewCount = RTInfo.MultiViewCount;
			}
		};

		FHashableStruct RTHash;
		FMemory::Memzero(RTHash);
		RTHash.Set(*this);
		return FCrc::MemCrc32(&RTHash, sizeof(RTHash));
	}
};

class FRHICustomPresent : public FRHIResource
{
public:
	FRHICustomPresent() : FRHIResource(RRT_CustomPresent) {}
	
	virtual ~FRHICustomPresent() {} // should release any references to D3D resources.
	
	// Called when viewport is resized.
	virtual void OnBackBufferResize() = 0;

	// Called from render thread to see if a native present will be requested for this frame.
	// @return	true if native Present will be requested for this frame; false otherwise.  Must
	// match value subsequently returned by Present for this frame.
	virtual bool NeedsNativePresent() = 0;
	// In come cases we want to use custom present but still let the native environment handle 
	// advancement of the backbuffer indices.
	// @return true if backbuffer index should advance independently from CustomPresent.
	virtual bool NeedsAdvanceBackbuffer() { return false; };

	// Called from RHI thread when the engine begins drawing to the viewport.
	virtual void BeginDrawing() {};

	// Called from RHI thread to perform custom present.
	// @param InOutSyncInterval - in out param, indicates if vsync is on (>0) or off (==0).
	// @return	true if native Present should be also be performed; false otherwise. If it returns
	// true, then InOutSyncInterval could be modified to switch between VSync/NoVSync for the normal 
	// Present.  Must match value previously returned by NeedsNativePresent for this frame.
	virtual bool Present(int32& InOutSyncInterval) = 0;

	// Called from RHI thread after native Present has been called
	virtual void PostPresent() {};

	// Called when rendering thread is acquired
	virtual void OnAcquireThreadOwnership() {}
	// Called when rendering thread is released
	virtual void OnReleaseThreadOwnership() {}
};


// Templates to convert an FRHI*Shader to its enum
template<typename TRHIShader> struct TRHIShaderToEnum {};
template<> struct TRHIShaderToEnum<FRHIVertexShader>           { enum { ShaderFrequency = SF_Vertex        }; };
template<> struct TRHIShaderToEnum<FRHIMeshShader>             { enum { ShaderFrequency = SF_Mesh          }; };
template<> struct TRHIShaderToEnum<FRHIAmplificationShader>    { enum { ShaderFrequency = SF_Amplification }; };
template<> struct TRHIShaderToEnum<FRHIPixelShader>            { enum { ShaderFrequency = SF_Pixel         }; };
template<> struct TRHIShaderToEnum<FRHIGeometryShader>         { enum { ShaderFrequency = SF_Geometry      }; };
template<> struct TRHIShaderToEnum<FRHIComputeShader>          { enum { ShaderFrequency = SF_Compute       }; };
template<> struct TRHIShaderToEnum<FRHIVertexShader*>          { enum { ShaderFrequency = SF_Vertex        }; };
template<> struct TRHIShaderToEnum<FRHIMeshShader*>            { enum { ShaderFrequency = SF_Mesh          }; };
template<> struct TRHIShaderToEnum<FRHIAmplificationShader*>   { enum { ShaderFrequency = SF_Amplification }; };
template<> struct TRHIShaderToEnum<FRHIPixelShader*>           { enum { ShaderFrequency = SF_Pixel         }; };
template<> struct TRHIShaderToEnum<FRHIGeometryShader*>        { enum { ShaderFrequency = SF_Geometry      }; };
template<> struct TRHIShaderToEnum<FRHIComputeShader*>         { enum { ShaderFrequency = SF_Compute       }; };
template<> struct TRHIShaderToEnum<FVertexShaderRHIRef>        { enum { ShaderFrequency = SF_Vertex        }; };
template<> struct TRHIShaderToEnum<FMeshShaderRHIRef>          { enum { ShaderFrequency = SF_Mesh          }; };
template<> struct TRHIShaderToEnum<FAmplificationShaderRHIRef> { enum { ShaderFrequency = SF_Amplification }; };
template<> struct TRHIShaderToEnum<FPixelShaderRHIRef>         { enum { ShaderFrequency = SF_Pixel         }; };
template<> struct TRHIShaderToEnum<FGeometryShaderRHIRef>      { enum { ShaderFrequency = SF_Geometry      }; };
template<> struct TRHIShaderToEnum<FComputeShaderRHIRef>       { enum { ShaderFrequency = SF_Compute       }; };

template<typename TRHIShaderType>
inline const TCHAR* GetShaderFrequencyString(bool bIncludePrefix = true)
{
	return GetShaderFrequencyString(static_cast<EShaderFrequency>(TRHIShaderToEnum<TRHIShaderType>::ShaderFrequency), bIncludePrefix);
}

struct FBoundShaderStateInput
{
	inline FBoundShaderStateInput() {}

	inline FBoundShaderStateInput
	(
		FRHIVertexDeclaration* InVertexDeclarationRHI
		, FRHIVertexShader* InVertexShaderRHI
		, FRHIPixelShader* InPixelShaderRHI
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		, FRHIGeometryShader* InGeometryShaderRHI
#endif
	)
		: VertexDeclarationRHI(InVertexDeclarationRHI)
		, VertexShaderRHI(InVertexShaderRHI)
		, PixelShaderRHI(InPixelShaderRHI)
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		, GeometryShaderRHI(InGeometryShaderRHI)
#endif
	{
	}

#if PLATFORM_SUPPORTS_MESH_SHADERS
	inline FBoundShaderStateInput(
		FRHIMeshShader* InMeshShaderRHI,
		FRHIAmplificationShader* InAmplificationShader,
		FRHIPixelShader* InPixelShaderRHI)
		: PixelShaderRHI(InPixelShaderRHI)
		, MeshShaderRHI(InMeshShaderRHI)
		, AmplificationShaderRHI(InAmplificationShader)
	{
	}
#endif

	void AddRefResources()
	{
		if (GetMeshShader())
		{
			check(VertexDeclarationRHI == nullptr);
			check(VertexShaderRHI == nullptr);
			GetMeshShader()->AddRef();

			if (GetAmplificationShader())
			{
				GetAmplificationShader()->AddRef();
			}
		}
		else
		{
			check(VertexDeclarationRHI);
			VertexDeclarationRHI->AddRef();

			check(VertexShaderRHI);
			VertexShaderRHI->AddRef();
		}

		if (PixelShaderRHI)
		{
			PixelShaderRHI->AddRef();
		}

		if (GetGeometryShader())
		{
			GetGeometryShader()->AddRef();
		}
	}

	void ReleaseResources()
	{
		if (GetMeshShader())
		{
			check(VertexDeclarationRHI == nullptr);
			check(VertexShaderRHI == nullptr);
			GetMeshShader()->Release();

			if (GetAmplificationShader())
			{
				GetAmplificationShader()->Release();
			}
		}
		else
		{
			check(VertexDeclarationRHI);
			VertexDeclarationRHI->Release();

			check(VertexShaderRHI);
			VertexShaderRHI->Release();
		}

		if (PixelShaderRHI)
		{
			PixelShaderRHI->Release();
		}

		if (GetGeometryShader())
		{
			GetGeometryShader()->Release();
		}
	}

	FRHIVertexShader* GetVertexShader() const { return VertexShaderRHI; }
	FRHIPixelShader* GetPixelShader() const { return PixelShaderRHI; }

#if PLATFORM_SUPPORTS_MESH_SHADERS
	FRHIMeshShader* GetMeshShader() const { return MeshShaderRHI; }
	void SetMeshShader(FRHIMeshShader* InMeshShader) { MeshShaderRHI = InMeshShader; }
	FRHIAmplificationShader* GetAmplificationShader() const { return AmplificationShaderRHI; }
	void SetAmplificationShader(FRHIAmplificationShader* InAmplificationShader) { AmplificationShaderRHI = InAmplificationShader; }
#else
	constexpr FRHIMeshShader* GetMeshShader() const { return nullptr; }
	void SetMeshShader(FRHIMeshShader*) {}
	constexpr FRHIAmplificationShader* GetAmplificationShader() const { return nullptr; }
	void SetAmplificationShader(FRHIAmplificationShader*) {}
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	FRHIGeometryShader* GetGeometryShader() const { return GeometryShaderRHI; }
	void SetGeometryShader(FRHIGeometryShader* InGeometryShader) { GeometryShaderRHI = InGeometryShader; }
#else
	constexpr FRHIGeometryShader* GetGeometryShader() const { return nullptr; }
	void SetGeometryShader(FRHIGeometryShader*) {}
#endif

	FRHIVertexDeclaration* VertexDeclarationRHI = nullptr;
	FRHIVertexShader* VertexShaderRHI = nullptr;
	FRHIPixelShader* PixelShaderRHI = nullptr;
private:
#if PLATFORM_SUPPORTS_MESH_SHADERS
	FRHIMeshShader* MeshShaderRHI = nullptr;
	FRHIAmplificationShader* AmplificationShaderRHI = nullptr;
#endif
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	FRHIGeometryShader* GeometryShaderRHI = nullptr;
#endif
};

// Hints for some RHIs that support subpasses
enum class ESubpassHint : uint8
{
	// Regular rendering
	None,

	// Render pass has depth reading subpass
	DepthReadSubpass,

	// Mobile defferred shading subpass
	DeferredShadingSubpass,

	// Mobile MSAA custom resolve subpass. Includes DepthReadSubpass.
	CustomResolveSubpass,
};

enum class EConservativeRasterization : uint8
{
	Disabled,
	Overestimated,
};

struct FGraphicsPipelineRenderTargetsInfo
{
	FGraphicsPipelineRenderTargetsInfo()
		: RenderTargetFormats(InPlace, UE_PIXELFORMAT_TO_UINT8(PF_Unknown))
		, RenderTargetFlags(InPlace, TexCreate_None)
		, DepthStencilAccess(FExclusiveDepthStencil::DepthNop_StencilNop)
	{
	}

	uint32															RenderTargetsEnabled = 0;
	TStaticArray<uint8, MaxSimultaneousRenderTargets>				RenderTargetFormats;
	TStaticArray<ETextureCreateFlags, MaxSimultaneousRenderTargets>	RenderTargetFlags;
	EPixelFormat													DepthStencilTargetFormat = PF_Unknown;
	ETextureCreateFlags												DepthStencilTargetFlag = ETextureCreateFlags::None;
	ERenderTargetLoadAction											DepthTargetLoadAction = ERenderTargetLoadAction::ENoAction;
	ERenderTargetStoreAction										DepthTargetStoreAction = ERenderTargetStoreAction::ENoAction;
	ERenderTargetLoadAction											StencilTargetLoadAction = ERenderTargetLoadAction::ENoAction;
	ERenderTargetStoreAction										StencilTargetStoreAction = ERenderTargetStoreAction::ENoAction;
	FExclusiveDepthStencil											DepthStencilAccess;
	uint16															NumSamples = 0;
	uint8															MultiViewCount = 0;
	bool															bHasFragmentDensityAttachment = false;
};


class FGraphicsPipelineStateInitializer
{
public:
	// Can't use TEnumByte<EPixelFormat> as it changes the struct to be non trivially constructible, breaking memset
	using TRenderTargetFormats		= TStaticArray<uint8/*EPixelFormat*/, MaxSimultaneousRenderTargets>;
	using TRenderTargetFlags		= TStaticArray<ETextureCreateFlags, MaxSimultaneousRenderTargets>;

	FGraphicsPipelineStateInitializer()
		: BlendState(nullptr)
		, RasterizerState(nullptr)
		, DepthStencilState(nullptr)
		, RenderTargetsEnabled(0)
		, RenderTargetFormats(InPlace, UE_PIXELFORMAT_TO_UINT8(PF_Unknown))
		, RenderTargetFlags(InPlace, TexCreate_None)
		, DepthStencilTargetFormat(PF_Unknown)
		, DepthStencilTargetFlag(TexCreate_None)
		, DepthTargetLoadAction(ERenderTargetLoadAction::ENoAction)
		, DepthTargetStoreAction(ERenderTargetStoreAction::ENoAction)
		, StencilTargetLoadAction(ERenderTargetLoadAction::ENoAction)
		, StencilTargetStoreAction(ERenderTargetStoreAction::ENoAction)
		, NumSamples(0)
		, SubpassHint(ESubpassHint::None)
		, SubpassIndex(0)
		, ConservativeRasterization(EConservativeRasterization::Disabled)
		, bDepthBounds(false)
		, MultiViewCount(0)
		, bHasFragmentDensityAttachment(false)
		, bAllowVariableRateShading(false)
		, ShadingRate(EVRSShadingRate::VRSSR_1x1)
		, Flags(0)
		, StatePrecachePSOHash(0)
	{
#if PLATFORM_WINDOWS
		static_assert(sizeof(TRenderTargetFormats::ElementType) == sizeof(uint8/*EPixelFormat*/), "Change TRenderTargetFormats's uint8 to EPixelFormat's size!");
#endif
		static_assert(PF_MAX < MAX_uint8, "TRenderTargetFormats assumes EPixelFormat can fit in a uint8!");
	}

	FGraphicsPipelineStateInitializer(
		FBoundShaderStateInput		InBoundShaderState,
		FRHIBlendState*				InBlendState,
		FRHIRasterizerState*		InRasterizerState,
		FRHIDepthStencilState*		InDepthStencilState,
		FImmutableSamplerState		InImmutableSamplerState,
		EPrimitiveType				InPrimitiveType,
		uint32						InRenderTargetsEnabled,
		const TRenderTargetFormats&	InRenderTargetFormats,
		const TRenderTargetFlags&	InRenderTargetFlags,
		EPixelFormat				InDepthStencilTargetFormat,
		ETextureCreateFlags			InDepthStencilTargetFlag,
		ERenderTargetLoadAction		InDepthTargetLoadAction,
		ERenderTargetStoreAction	InDepthTargetStoreAction,
		ERenderTargetLoadAction		InStencilTargetLoadAction,
		ERenderTargetStoreAction	InStencilTargetStoreAction,
		FExclusiveDepthStencil		InDepthStencilAccess,
		uint16						InNumSamples,
		ESubpassHint				InSubpassHint,
		uint8						InSubpassIndex,
		EConservativeRasterization	InConservativeRasterization,
		uint16						InFlags,
		bool						bInDepthBounds,
		uint8						InMultiViewCount,
		bool						bInHasFragmentDensityAttachment,
		bool						bInAllowVariableRateShading,
		EVRSShadingRate				InShadingRate)
		: BoundShaderState(InBoundShaderState)
		, BlendState(InBlendState)
		, RasterizerState(InRasterizerState)
		, DepthStencilState(InDepthStencilState)
		, ImmutableSamplerState(InImmutableSamplerState)
		, PrimitiveType(InPrimitiveType)
		, RenderTargetsEnabled(InRenderTargetsEnabled)
		, RenderTargetFormats(InRenderTargetFormats)
		, RenderTargetFlags(InRenderTargetFlags)
		, DepthStencilTargetFormat(InDepthStencilTargetFormat)
		, DepthStencilTargetFlag(InDepthStencilTargetFlag)
		, DepthTargetLoadAction(InDepthTargetLoadAction)
		, DepthTargetStoreAction(InDepthTargetStoreAction)
		, StencilTargetLoadAction(InStencilTargetLoadAction)
		, StencilTargetStoreAction(InStencilTargetStoreAction)
		, DepthStencilAccess(InDepthStencilAccess)
		, NumSamples(InNumSamples)
		, SubpassHint(InSubpassHint)
		, SubpassIndex(InSubpassIndex)
		, ConservativeRasterization(EConservativeRasterization::Disabled)
		, bDepthBounds(bInDepthBounds)
		, MultiViewCount(InMultiViewCount)
		, bHasFragmentDensityAttachment(bInHasFragmentDensityAttachment)
		, bAllowVariableRateShading(bInAllowVariableRateShading)
		, ShadingRate(InShadingRate)
		, Flags(InFlags)
		, StatePrecachePSOHash(0)
	{
	}

	bool operator==(const FGraphicsPipelineStateInitializer& rhs) const
	{
		if (BoundShaderState.VertexDeclarationRHI != rhs.BoundShaderState.VertexDeclarationRHI ||
			BoundShaderState.VertexShaderRHI != rhs.BoundShaderState.VertexShaderRHI ||
			BoundShaderState.PixelShaderRHI != rhs.BoundShaderState.PixelShaderRHI ||
			BoundShaderState.GetMeshShader() != rhs.BoundShaderState.GetMeshShader() ||
			BoundShaderState.GetAmplificationShader() != rhs.BoundShaderState.GetAmplificationShader() ||
			BoundShaderState.GetGeometryShader() != rhs.BoundShaderState.GetGeometryShader() ||
			BlendState != rhs.BlendState ||
			RasterizerState != rhs.RasterizerState ||
			DepthStencilState != rhs.DepthStencilState ||
			ImmutableSamplerState != rhs.ImmutableSamplerState ||
			PrimitiveType != rhs.PrimitiveType ||
			bDepthBounds != rhs.bDepthBounds ||
			MultiViewCount != rhs.MultiViewCount ||
			ShadingRate != rhs.ShadingRate ||
			bAllowVariableRateShading != rhs.bAllowVariableRateShading ||
			bHasFragmentDensityAttachment != rhs.bHasFragmentDensityAttachment ||
			RenderTargetsEnabled != rhs.RenderTargetsEnabled ||
			RenderTargetFormats != rhs.RenderTargetFormats || 
			!RelevantRenderTargetFlagsEqual(RenderTargetFlags, rhs.RenderTargetFlags) || 
			DepthStencilTargetFormat != rhs.DepthStencilTargetFormat || 
			!RelevantDepthStencilFlagsEqual(DepthStencilTargetFlag, rhs.DepthStencilTargetFlag) ||
			DepthTargetLoadAction != rhs.DepthTargetLoadAction ||
			DepthTargetStoreAction != rhs.DepthTargetStoreAction ||
			StencilTargetLoadAction != rhs.StencilTargetLoadAction ||
			StencilTargetStoreAction != rhs.StencilTargetStoreAction || 
			DepthStencilAccess != rhs.DepthStencilAccess ||
			NumSamples != rhs.NumSamples ||
			SubpassHint != rhs.SubpassHint ||
			SubpassIndex != rhs.SubpassIndex ||
			ConservativeRasterization != rhs.ConservativeRasterization)
		{
			return false;
		}

		return true;
	}

	// We care about flags that influence RT formats (which is the only thing the underlying API cares about).
	// In most RHIs, the format is only influenced by TexCreate_SRGB. D3D12 additionally uses TexCreate_Shared in its format selection logic.
	static constexpr ETextureCreateFlags RelevantRenderTargetFlagMask = ETextureCreateFlags::SRGB | ETextureCreateFlags::Shared;

	// We care about flags that influence DS formats (which is the only thing the underlying API cares about).
	// D3D12 shares the format choice function with the RT, so preserving all the flags used there out of abundance of caution.
	static constexpr ETextureCreateFlags RelevantDepthStencilFlagMask = ETextureCreateFlags::SRGB | ETextureCreateFlags::Shared | ETextureCreateFlags::DepthStencilTargetable;

	static bool RelevantRenderTargetFlagsEqual(const TRenderTargetFlags& A, const TRenderTargetFlags& B)
	{
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			ETextureCreateFlags FlagsA = A[Index] & RelevantRenderTargetFlagMask;
			ETextureCreateFlags FlagsB = B[Index] & RelevantRenderTargetFlagMask;
			if (FlagsA != FlagsB)
			{
				return false;
			}
		}
		return true;
	}

	static bool RelevantDepthStencilFlagsEqual(const ETextureCreateFlags A, const ETextureCreateFlags B)
	{
		ETextureCreateFlags FlagsA = (A & RelevantDepthStencilFlagMask);
		ETextureCreateFlags FlagsB = (B & RelevantDepthStencilFlagMask);
		return (FlagsA == FlagsB);
	}

	uint32 ComputeNumValidRenderTargets() const
	{
		// Get the count of valid render targets (ignore those at the end of the array with PF_Unknown)
		if (RenderTargetsEnabled > 0)
		{
			int32 LastValidTarget = -1;
			for (int32 i = (int32)RenderTargetsEnabled - 1; i >= 0; i--)
			{
				if (RenderTargetFormats[i] != PF_Unknown)
				{
					LastValidTarget = i;
					break;
				}
			}
			return uint32(LastValidTarget + 1);
		}
		return RenderTargetsEnabled;
	}

	FBoundShaderStateInput			BoundShaderState;
	FRHIBlendState*					BlendState;
	FRHIRasterizerState*			RasterizerState;
	FRHIDepthStencilState*			DepthStencilState;
	FImmutableSamplerState			ImmutableSamplerState;

	EPrimitiveType					PrimitiveType;
	uint32							RenderTargetsEnabled;
	TRenderTargetFormats			RenderTargetFormats;
	TRenderTargetFlags				RenderTargetFlags;
	EPixelFormat					DepthStencilTargetFormat;
	ETextureCreateFlags				DepthStencilTargetFlag;
	ERenderTargetLoadAction			DepthTargetLoadAction;
	ERenderTargetStoreAction		DepthTargetStoreAction;
	ERenderTargetLoadAction			StencilTargetLoadAction;
	ERenderTargetStoreAction		StencilTargetStoreAction;
	FExclusiveDepthStencil			DepthStencilAccess;
	uint16							NumSamples;
	ESubpassHint					SubpassHint;
	uint8							SubpassIndex;
	EConservativeRasterization		ConservativeRasterization;
	bool							bDepthBounds;
	uint8							MultiViewCount;
	bool							bHasFragmentDensityAttachment;
	bool							bAllowVariableRateShading;
	EVRSShadingRate					ShadingRate;
	
	// Note: these flags do NOT affect compilation of this PSO.
	// The resulting object is invariant with respect to whatever is set here, they are
	// behavior hints.
	// They do not participate in equality comparisons or hashing.
	union
	{
		struct
		{
			uint16					Reserved			: 14;
			uint16					bPSOPrecache			: 1;
			uint16					bFromPSOFileCache	: 1;
		};
		uint16						Flags;
	};

	// Cached hash off all state data provided at creation time (Only contains hash of data which influences the PSO precaching for the current platform)
	// Created from hashing the state data instead of the pointers which are used during fast runtime cache checking and compares
	uint64							StatePrecachePSOHash;
};

class FRayTracingPipelineStateSignature
{
public:

	uint32 MaxAttributeSizeInBytes = 8; // sizeof FRayTracingIntersectionAttributes declared in RayTracingCommon.ush
	uint32 MaxPayloadSizeInBytes = 24; // sizeof FDefaultPayload declared in RayTracingCommon.ush
	bool bAllowHitGroupIndexing = true;

	// NOTE: GetTypeHash(const FRayTracingPipelineStateInitializer& Initializer) should also be updated when changing this function
	bool operator==(const FRayTracingPipelineStateSignature& rhs) const
	{
		return MaxAttributeSizeInBytes == rhs.MaxAttributeSizeInBytes
			&& MaxPayloadSizeInBytes == rhs.MaxPayloadSizeInBytes
			&& bAllowHitGroupIndexing == rhs.bAllowHitGroupIndexing
			&& RayGenHash == rhs.RayGenHash
			&& MissHash == rhs.MissHash
			&& HitGroupHash == rhs.HitGroupHash
			&& CallableHash == rhs.CallableHash;
	}

	friend uint32 GetTypeHash(const FRayTracingPipelineStateSignature& Initializer)
	{
		return GetTypeHash(Initializer.MaxAttributeSizeInBytes) ^
			GetTypeHash(Initializer.MaxPayloadSizeInBytes) ^
			GetTypeHash(Initializer.bAllowHitGroupIndexing) ^
			GetTypeHash(Initializer.GetRayGenHash()) ^
			GetTypeHash(Initializer.GetRayMissHash()) ^
			GetTypeHash(Initializer.GetHitGroupHash()) ^
			GetTypeHash(Initializer.GetCallableHash());
	}

	uint64 GetHitGroupHash() const { return HitGroupHash; }
	uint64 GetRayGenHash()   const { return RayGenHash; }
	uint64 GetRayMissHash()  const { return MissHash; }
	uint64 GetCallableHash() const { return CallableHash; }

protected:

	uint64 RayGenHash = 0;
	uint64 MissHash = 0;
	uint64 HitGroupHash = 0;
	uint64 CallableHash = 0;
};

class FRayTracingPipelineStateInitializer : public FRayTracingPipelineStateSignature
{
public:

	FRayTracingPipelineStateInitializer() = default;

	// Partial ray tracing pipelines can be used for run-time asynchronous shader compilation, but not for rendering.
	// Any number of shaders for any stage may be provided when creating partial pipelines, but 
	// at least one shader must be present in total (completely empty pipelines are not allowed).
	bool bPartial = false;

	// Hints to the RHI that this PSO is being compiled by a background task and will not be needed immediately for rendering.
	// Speculative PSO pre-caching or non-blocking PSO creation should set this flag.
	// This may be used by the RHI to decide if a hitch warning should be reported, change priority of any internally dispatched tasks, etc.
	// Does not affect the creation of the PSO itself.
	bool bBackgroundCompilation = false;

	// Ray tracing pipeline may be created by deriving from the existing base.
	// Base pipeline will be extended by adding new shaders into it, potentially saving substantial amount of CPU time.
	// Depends on GRHISupportsRayTracingPSOAdditions support at runtime (base pipeline is simply ignored if it is unsupported).
	FRayTracingPipelineStateRHIRef BasePipeline;

	const TArrayView<FRHIRayTracingShader*>& GetRayGenTable()   const { return RayGenTable; }
	const TArrayView<FRHIRayTracingShader*>& GetMissTable()     const { return MissTable; }
	const TArrayView<FRHIRayTracingShader*>& GetHitGroupTable() const { return HitGroupTable; }
	const TArrayView<FRHIRayTracingShader*>& GetCallableTable() const { return CallableTable; }

	// Shaders used as entry point to ray tracing work. At least one RayGen shader must be provided.
	void SetRayGenShaderTable(const TArrayView<FRHIRayTracingShader*>& InRayGenShaders, uint64 Hash = 0)
	{
		RayGenTable = InRayGenShaders;
		RayGenHash = Hash ? Hash : ComputeShaderTableHash(InRayGenShaders);
	}

	// Shaders that will be invoked if a ray misses all geometry.
	// If this table is empty, then a built-in default miss shader will be used that sets HitT member of FMinimalPayload to -1.
	// Desired miss shader can be selected by providing MissShaderIndex to TraceRay() function.
	void SetMissShaderTable(const TArrayView<FRHIRayTracingShader*>& InMissShaders, uint64 Hash = 0)
	{
		MissTable = InMissShaders;
		MissHash = Hash ? Hash : ComputeShaderTableHash(InMissShaders);
	}

	// Shaders that will be invoked when ray intersects geometry.
	// If this table is empty, then a built-in default shader will be used for all geometry, using FDefaultPayload.
	void SetHitGroupTable(const TArrayView<FRHIRayTracingShader*>& InHitGroups, uint64 Hash = 0)
	{
		HitGroupTable = InHitGroups;
		HitGroupHash = Hash ? Hash : ComputeShaderTableHash(HitGroupTable);
	}

	// Shaders that can be explicitly invoked from RayGen shaders by their Shader Binding Table (SBT) index.
	// SetRayTracingCallableShader() command must be used to fill SBT slots before a shader can be called.
	void SetCallableTable(const TArrayView<FRHIRayTracingShader*>& InCallableShaders, uint64 Hash = 0)
	{
		CallableTable = InCallableShaders;
		CallableHash = Hash ? Hash : ComputeShaderTableHash(CallableTable);
	}

private:

	uint64 ComputeShaderTableHash(const TArrayView<FRHIRayTracingShader*>& ShaderTable, uint64 InitialHash = 5699878132332235837ull)
	{
		uint64 CombinedHash = InitialHash;
		for (FRHIRayTracingShader* ShaderRHI : ShaderTable)
		{
			uint64 ShaderHash; // 64 bits from the shader SHA1
			FMemory::Memcpy(&ShaderHash, ShaderRHI->GetHash().Hash, sizeof(ShaderHash));

			// 64 bit hash combination as per boost::hash_combine_impl
			CombinedHash ^= ShaderHash + 0x9e3779b9 + (CombinedHash << 6) + (CombinedHash >> 2);
		}

		return CombinedHash;
	}

	TArrayView<FRHIRayTracingShader*> RayGenTable;
	TArrayView<FRHIRayTracingShader*> MissTable;
	TArrayView<FRHIRayTracingShader*> HitGroupTable;
	TArrayView<FRHIRayTracingShader*> CallableTable;
};

// This PSO is used as a fallback for RHIs that dont support PSOs. It is used to set the graphics state using the legacy state setting APIs
class FRHIGraphicsPipelineStateFallBack : public FRHIGraphicsPipelineState
{
public:
	FRHIGraphicsPipelineStateFallBack() {}

	FRHIGraphicsPipelineStateFallBack(const FGraphicsPipelineStateInitializer& Init)
		: Initializer(Init)
	{
	}

	FGraphicsPipelineStateInitializer Initializer;
};

class FRHIComputePipelineStateFallback : public FRHIComputePipelineState
{
public:
	FRHIComputePipelineStateFallback(FRHIComputeShader* InComputeShader)
		: ComputeShader(InComputeShader)
	{
		check(InComputeShader);
	}

	FRHIComputeShader* GetComputeShader()
	{
		return ComputeShader;
	}

protected:
	TRefCountPtr<FRHIComputeShader> ComputeShader;
};

enum class ERenderTargetActions : uint8
{
	LoadOpMask = 2,

#define RTACTION_MAKE_MASK(Load, Store) (((uint8)ERenderTargetLoadAction::Load << (uint8)LoadOpMask) | (uint8)ERenderTargetStoreAction::Store)

	DontLoad_DontStore =	RTACTION_MAKE_MASK(ENoAction, ENoAction),

	DontLoad_Store =		RTACTION_MAKE_MASK(ENoAction, EStore),
	Clear_Store =			RTACTION_MAKE_MASK(EClear, EStore),
	Load_Store =			RTACTION_MAKE_MASK(ELoad, EStore),

	Clear_DontStore =		RTACTION_MAKE_MASK(EClear, ENoAction),
	Load_DontStore =		RTACTION_MAKE_MASK(ELoad, ENoAction),
	Clear_Resolve =			RTACTION_MAKE_MASK(EClear, EMultisampleResolve),
	Load_Resolve =			RTACTION_MAKE_MASK(ELoad, EMultisampleResolve),

#undef RTACTION_MAKE_MASK
};

inline ERenderTargetActions MakeRenderTargetActions(ERenderTargetLoadAction Load, ERenderTargetStoreAction Store)
{
	return (ERenderTargetActions)(((uint8)Load << (uint8)ERenderTargetActions::LoadOpMask) | (uint8)Store);
}

inline ERenderTargetLoadAction GetLoadAction(ERenderTargetActions Action)
{
	return (ERenderTargetLoadAction)((uint8)Action >> (uint8)ERenderTargetActions::LoadOpMask);
}

inline ERenderTargetStoreAction GetStoreAction(ERenderTargetActions Action)
{
	return (ERenderTargetStoreAction)((uint8)Action & ((1 << (uint8)ERenderTargetActions::LoadOpMask) - 1));
}

enum class EDepthStencilTargetActions : uint8
{
	DepthMask = 4,

#define RTACTION_MAKE_MASK(Depth, Stencil) (((uint8)ERenderTargetActions::Depth << (uint8)DepthMask) | (uint8)ERenderTargetActions::Stencil)

	DontLoad_DontStore =						RTACTION_MAKE_MASK(DontLoad_DontStore, DontLoad_DontStore),
	DontLoad_StoreDepthStencil =				RTACTION_MAKE_MASK(DontLoad_Store, DontLoad_Store),
	DontLoad_StoreStencilNotDepth =				RTACTION_MAKE_MASK(DontLoad_DontStore, DontLoad_Store),
	ClearDepthStencil_StoreDepthStencil =		RTACTION_MAKE_MASK(Clear_Store, Clear_Store),
	LoadDepthStencil_StoreDepthStencil =		RTACTION_MAKE_MASK(Load_Store, Load_Store),
	LoadDepthNotStencil_StoreDepthNotStencil =	RTACTION_MAKE_MASK(Load_Store, DontLoad_DontStore),
	LoadDepthNotStencil_DontStore =				RTACTION_MAKE_MASK(Load_DontStore, DontLoad_DontStore),
	LoadDepthStencil_StoreStencilNotDepth =		RTACTION_MAKE_MASK(Load_DontStore, Load_Store),

	ClearDepthStencil_DontStoreDepthStencil =	RTACTION_MAKE_MASK(Clear_DontStore, Clear_DontStore),
	LoadDepthStencil_DontStoreDepthStencil =	RTACTION_MAKE_MASK(Load_DontStore, Load_DontStore),
	ClearDepthStencil_StoreDepthNotStencil =	RTACTION_MAKE_MASK(Clear_Store, Clear_DontStore),
	ClearDepthStencil_StoreStencilNotDepth =	RTACTION_MAKE_MASK(Clear_DontStore, Clear_Store),
	ClearDepthStencil_ResolveDepthNotStencil =	RTACTION_MAKE_MASK(Clear_Resolve, Clear_DontStore),
	ClearDepthStencil_ResolveStencilNotDepth =	RTACTION_MAKE_MASK(Clear_DontStore, Clear_Resolve),
	LoadDepthClearStencil_StoreDepthStencil  =  RTACTION_MAKE_MASK(Load_Store, Clear_Store),

	ClearStencilDontLoadDepth_StoreStencilNotDepth = RTACTION_MAKE_MASK(DontLoad_DontStore, Clear_Store),

#undef RTACTION_MAKE_MASK
};

inline constexpr EDepthStencilTargetActions MakeDepthStencilTargetActions(const ERenderTargetActions Depth, const ERenderTargetActions Stencil)
{
	return (EDepthStencilTargetActions)(((uint8)Depth << (uint8)EDepthStencilTargetActions::DepthMask) | (uint8)Stencil);
}

inline ERenderTargetActions GetDepthActions(EDepthStencilTargetActions Action)
{
	return (ERenderTargetActions)((uint8)Action >> (uint8)EDepthStencilTargetActions::DepthMask);
}

inline ERenderTargetActions GetStencilActions(EDepthStencilTargetActions Action)
{
	return (ERenderTargetActions)((uint8)Action & ((1 << (uint8)EDepthStencilTargetActions::DepthMask) - 1));
}

struct FResolveRect
{
	int32 X1;
	int32 Y1;
	int32 X2;
	int32 Y2;

	// e.g. for a a full 256 x 256 area starting at (0, 0) it would be 
	// the values would be 0, 0, 256, 256
	FResolveRect(int32 InX1 = -1, int32 InY1 = -1, int32 InX2 = -1, int32 InY2 = -1)
		: X1(InX1)
		, Y1(InY1)
		, X2(InX2)
		, Y2(InY2)
	{}

	explicit FResolveRect(FIntRect Other)
		: X1(Other.Min.X)
		, Y1(Other.Min.Y)
		, X2(Other.Max.X)
		, Y2(Other.Max.Y)
	{}

	bool operator==(FResolveRect Other) const
	{
		return X1 == Other.X1 && Y1 == Other.Y1 && X2 == Other.X2 && Y2 == Other.Y2;
	}

	bool operator!=(FResolveRect Other) const
	{
		return !(*this == Other);
	}

	bool IsValid() const
	{
		return X1 >= 0 && Y1 >= 0 && X2 - X1 > 0 && Y2 - Y1 > 0;
	}
};

struct FRHIRenderPassInfo
{
	struct FColorEntry
	{
		FRHITexture*         RenderTarget      = nullptr;
		FRHITexture*         ResolveTarget     = nullptr;
		int32                ArraySlice        = -1;
		uint8                MipIndex          = 0;
		ERenderTargetActions Action            = ERenderTargetActions::DontLoad_DontStore;
	};
	TStaticArray<FColorEntry, MaxSimultaneousRenderTargets> ColorRenderTargets;

	struct FDepthStencilEntry
	{
		FRHITexture*         DepthStencilTarget = nullptr;
		FRHITexture*         ResolveTarget      = nullptr;
		EDepthStencilTargetActions Action       = EDepthStencilTargetActions::DontLoad_DontStore;
		FExclusiveDepthStencil ExclusiveDepthStencil;
	};
	FDepthStencilEntry DepthStencilRenderTarget;

	// Controls the area for a multisample resolve or raster UAV (i.e. no fixed-function targets) operation.
	FResolveRect ResolveRect;

	// Some RHIs can use a texture to control the sampling and/or shading resolution of different areas 
	FTextureRHIRef ShadingRateTexture = nullptr;
	EVRSRateCombiner ShadingRateTextureCombiner = VRSRB_Passthrough;

	// Some RHIs require a hint that occlusion queries will be used in this render pass
	uint32 NumOcclusionQueries = 0;
	bool bOcclusionQueries = false;

	// if this renderpass should be multiview, and if so how many views are required
	uint8 MultiViewCount = 0;

	// Hint for some RHI's that renderpass will have specific sub-passes 
	ESubpassHint SubpassHint = ESubpassHint::None;

	FRHIRenderPassInfo() = default;
	FRHIRenderPassInfo(const FRHIRenderPassInfo&) = default;
	FRHIRenderPassInfo& operator=(const FRHIRenderPassInfo&) = default;

	// Color, no depth, optional resolve, optional mip, optional array slice
	explicit FRHIRenderPassInfo(FRHITexture* ColorRT, ERenderTargetActions ColorAction, FRHITexture* ResolveRT = nullptr, uint8 InMipIndex = 0, int32 InArraySlice = -1)
	{
		check(!(ResolveRT && ResolveRT->IsMultisampled()));
		check(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = ResolveRT;
		ColorRenderTargets[0].ArraySlice = InArraySlice;
		ColorRenderTargets[0].MipIndex = InMipIndex;
		ColorRenderTargets[0].Action = ColorAction;
	}

	// Color MRTs, no depth
	explicit FRHIRenderPassInfo(int32 NumColorRTs, FRHITexture* ColorRTs[], ERenderTargetActions ColorAction)
	{
		check(NumColorRTs > 0);
		for (int32 Index = 0; Index < NumColorRTs; ++Index)
		{
			check(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ArraySlice = -1;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		DepthStencilRenderTarget.DepthStencilTarget = nullptr;
		DepthStencilRenderTarget.Action = EDepthStencilTargetActions::DontLoad_DontStore;
		DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthNop_StencilNop;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
	}

	// Color MRTs, no depth
	explicit FRHIRenderPassInfo(int32 NumColorRTs, FRHITexture* ColorRTs[], ERenderTargetActions ColorAction, FRHITexture* ResolveTargets[])
	{
		check(NumColorRTs > 0);
		for (int32 Index = 0; Index < NumColorRTs; ++Index)
		{
			check(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ResolveTarget = ResolveTargets[Index];
			ColorRenderTargets[Index].ArraySlice = -1;
			ColorRenderTargets[Index].MipIndex = 0;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		DepthStencilRenderTarget.DepthStencilTarget = nullptr;
		DepthStencilRenderTarget.Action = EDepthStencilTargetActions::DontLoad_DontStore;
		DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthNop_StencilNop;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
	}

	// Color MRTs and depth
	explicit FRHIRenderPassInfo(int32 NumColorRTs, FRHITexture* ColorRTs[], ERenderTargetActions ColorAction, FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		check(NumColorRTs > 0);
		for (int32 Index = 0; Index < NumColorRTs; ++Index)
		{
			check(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ResolveTarget = nullptr;
			ColorRenderTargets[Index].ArraySlice = -1;
			ColorRenderTargets[Index].MipIndex = 0;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
	}

	// Color MRTs and depth
	explicit FRHIRenderPassInfo(int32 NumColorRTs, FRHITexture* ColorRTs[], ERenderTargetActions ColorAction, FRHITexture* ResolveRTs[], FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions, FRHITexture* ResolveDepthRT, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		check(NumColorRTs > 0);
		for (int32 Index = 0; Index < NumColorRTs; ++Index)
		{
			check(!ResolveRTs[Index] || ResolveRTs[Index]->IsMultisampled());
			check(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ResolveTarget = ResolveRTs[Index];
			ColorRenderTargets[Index].ArraySlice = -1;
			ColorRenderTargets[Index].MipIndex = 0;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		check(!ResolveDepthRT || ResolveDepthRT->IsMultisampled());
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
	}

	// Depth, no color
	explicit FRHIRenderPassInfo(FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions, FRHITexture* ResolveDepthRT = nullptr, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		check(!ResolveDepthRT || ResolveDepthRT->IsMultisampled());
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
	}

	// Depth, no color, occlusion queries
	explicit FRHIRenderPassInfo(FRHITexture* DepthRT, uint32 InNumOcclusionQueries, EDepthStencilTargetActions DepthActions, FRHITexture* ResolveDepthRT = nullptr, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
		: NumOcclusionQueries(InNumOcclusionQueries)
	{
		check(!ResolveDepthRT || ResolveDepthRT->IsMultisampled());
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
	}

	// Color and depth
	explicit FRHIRenderPassInfo(FRHITexture* ColorRT, ERenderTargetActions ColorAction, FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		check(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = nullptr;
		ColorRenderTargets[0].ArraySlice = -1;
		ColorRenderTargets[0].MipIndex = 0;
		ColorRenderTargets[0].Action = ColorAction;
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
		FMemory::Memzero(&ColorRenderTargets[1], sizeof(FColorEntry) * (MaxSimultaneousRenderTargets - 1));
	}

	// Color and depth with resolve
	explicit FRHIRenderPassInfo(FRHITexture* ColorRT, ERenderTargetActions ColorAction, FRHITexture* ResolveColorRT,
		FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions, FRHITexture* ResolveDepthRT, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		check(!ResolveColorRT || ResolveColorRT->IsMultisampled());
		check(!ResolveDepthRT || ResolveDepthRT->IsMultisampled());
		check(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = ResolveColorRT;
		ColorRenderTargets[0].ArraySlice = -1;
		ColorRenderTargets[0].MipIndex = 0;
		ColorRenderTargets[0].Action = ColorAction;
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
		FMemory::Memzero(&ColorRenderTargets[1], sizeof(FColorEntry) * (MaxSimultaneousRenderTargets - 1));
	}

	// Color and depth with resolve and optional sample density
	explicit FRHIRenderPassInfo(FRHITexture* ColorRT, ERenderTargetActions ColorAction, FRHITexture* ResolveColorRT,
		FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions, FRHITexture* ResolveDepthRT, 
		FRHITexture* InShadingRateTexture, EVRSRateCombiner InShadingRateTextureCombiner,
		FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		check(!ResolveColorRT || ResolveColorRT->IsMultisampled());
		check(!ResolveDepthRT || ResolveDepthRT->IsMultisampled());
		check(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = ResolveColorRT;
		ColorRenderTargets[0].ArraySlice = -1;
		ColorRenderTargets[0].MipIndex = 0;
		ColorRenderTargets[0].Action = ColorAction;
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
		ShadingRateTexture = InShadingRateTexture;
		ShadingRateTextureCombiner = InShadingRateTextureCombiner;
		FMemory::Memzero(&ColorRenderTargets[1], sizeof(FColorEntry) * (MaxSimultaneousRenderTargets - 1));
	}

	inline int32 GetNumColorRenderTargets() const
	{
		int32 ColorIndex = 0;
		for (; ColorIndex < MaxSimultaneousRenderTargets; ++ColorIndex)
		{
			const FColorEntry& Entry = ColorRenderTargets[ColorIndex];
			if (!Entry.RenderTarget)
			{
				break;
			}
		}

		return ColorIndex;
	}

	FGraphicsPipelineRenderTargetsInfo ExtractRenderTargetsInfo() const
	{
		FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;

		RenderTargetsInfo.NumSamples = 1;
		int32 RenderTargetIndex = 0;

		for (; RenderTargetIndex < MaxSimultaneousRenderTargets; ++RenderTargetIndex)
		{
			FRHITexture* RenderTarget = ColorRenderTargets[RenderTargetIndex].RenderTarget;
			if (!RenderTarget)
			{
				break;
			}

			RenderTargetsInfo.RenderTargetFormats[RenderTargetIndex] = (uint8)RenderTarget->GetFormat();
			RenderTargetsInfo.RenderTargetFlags[RenderTargetIndex] = RenderTarget->GetFlags();
			RenderTargetsInfo.NumSamples |= RenderTarget->GetNumSamples();
		}

		RenderTargetsInfo.RenderTargetsEnabled = RenderTargetIndex;
		for (; RenderTargetIndex < MaxSimultaneousRenderTargets; ++RenderTargetIndex)
		{
			RenderTargetsInfo.RenderTargetFormats[RenderTargetIndex] = PF_Unknown;
		}

		if (DepthStencilRenderTarget.DepthStencilTarget)
		{
			RenderTargetsInfo.DepthStencilTargetFormat = DepthStencilRenderTarget.DepthStencilTarget->GetFormat();
			RenderTargetsInfo.DepthStencilTargetFlag = DepthStencilRenderTarget.DepthStencilTarget->GetFlags();
			RenderTargetsInfo.NumSamples |= DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples();
		}
		else
		{
			RenderTargetsInfo.DepthStencilTargetFormat = PF_Unknown;
		}

		const ERenderTargetActions DepthActions = GetDepthActions(DepthStencilRenderTarget.Action);
		const ERenderTargetActions StencilActions = GetStencilActions(DepthStencilRenderTarget.Action);
		RenderTargetsInfo.DepthTargetLoadAction = GetLoadAction(DepthActions);
		RenderTargetsInfo.DepthTargetStoreAction = GetStoreAction(DepthActions);
		RenderTargetsInfo.StencilTargetLoadAction = GetLoadAction(StencilActions);
		RenderTargetsInfo.StencilTargetStoreAction = GetStoreAction(StencilActions);
		RenderTargetsInfo.DepthStencilAccess = DepthStencilRenderTarget.ExclusiveDepthStencil;

		RenderTargetsInfo.MultiViewCount = MultiViewCount;
		RenderTargetsInfo.bHasFragmentDensityAttachment = ShadingRateTexture != nullptr;

		return RenderTargetsInfo;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	RHI_API void Validate() const;
#else
	void Validate() const {}
#endif
	RHI_API void ConvertToRenderTargetsInfo(FRHISetRenderTargetsInfo& OutRTInfo) const;
};

//UE_DEPRECATED(5.3, "Use FRHITextureSRVCreateDesc::SetDisableSRGB to create a view which explicitly disables SRGB.")
enum ERHITextureSRVOverrideSRGBType : uint8
{
	SRGBO_Default,
	SRGBO_ForceDisable,
};

//UE_DEPRECATED(5.3, "Use FRHITextureSRVCreateDesc rather than FRHITextureSRVCreateInfo.")
struct FRHITextureSRVCreateInfo
{
	explicit FRHITextureSRVCreateInfo(uint8 InMipLevel = 0u, uint8 InNumMipLevels = 1u, EPixelFormat InFormat = PF_Unknown)
		: Format(InFormat)
		, MipLevel(InMipLevel)
		, NumMipLevels(InNumMipLevels)
		, SRGBOverride(SRGBO_Default)
		, FirstArraySlice(0)
		, NumArraySlices(0)
	{}

	explicit FRHITextureSRVCreateInfo(uint8 InMipLevel, uint8 InNumMipLevels, uint16 InFirstArraySlice, uint16 InNumArraySlices, EPixelFormat InFormat = PF_Unknown)
		: Format(InFormat)
		, MipLevel(InMipLevel)
		, NumMipLevels(InNumMipLevels)
		, SRGBOverride(SRGBO_Default)
		, FirstArraySlice(InFirstArraySlice)
		, NumArraySlices(InNumArraySlices)
	{}

	/** View the texture with a different format. Leave as PF_Unknown to use original format. Useful when sampling stencil */
	EPixelFormat Format;

	/** Specify the mip level to use. Useful when rendering to one mip while sampling from another */
	uint8 MipLevel;

	/** Create a view to a single, or multiple mip levels */
	uint8 NumMipLevels;

	/** Potentially override the texture's sRGB flag */
	ERHITextureSRVOverrideSRGBType SRGBOverride;

	/** Specify first array slice index. By default 0. */
	uint16 FirstArraySlice;

	/** Specify number of array slices. If FirstArraySlice and NumArraySlices are both zero, the SRV is created for all array slices. By default 0. */
	uint16 NumArraySlices;

	/** Specify the metadata plane to use when creating a view. */
	ERHITextureMetaDataAccess MetaData = ERHITextureMetaDataAccess::None;
    
    /** Specify a dimension to use which overrides the default  */
    TOptional<ETextureDimension> DimensionOverride;

	FORCEINLINE bool operator==(const FRHITextureSRVCreateInfo& Other)const
	{
		return (
			Format == Other.Format &&
			MipLevel == Other.MipLevel &&
			NumMipLevels == Other.NumMipLevels &&
			SRGBOverride == Other.SRGBOverride &&
			FirstArraySlice == Other.FirstArraySlice &&
			NumArraySlices == Other.NumArraySlices &&
			MetaData == Other.MetaData &&
            DimensionOverride == Other.DimensionOverride);
	}

	FORCEINLINE bool operator!=(const FRHITextureSRVCreateInfo& Other)const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FRHITextureSRVCreateInfo& Info)
	{
		uint32 Hash = uint32(Info.Format) | uint32(Info.MipLevel) << 8 | uint32(Info.NumMipLevels) << 16 | uint32(Info.SRGBOverride) << 24;
		Hash = HashCombine(Hash, uint32(Info.FirstArraySlice) | uint32(Info.NumArraySlices) << 16);
        Hash = HashCombine(Hash, Info.DimensionOverride.IsSet() ? uint32(*Info.DimensionOverride) : MAX_uint32);
		Hash = HashCombine(Hash, uint32(Info.MetaData));
		return Hash;
	}

	/** Check the validity. */
	static bool CheckValidity(const FRHITextureDesc& TextureDesc, const FRHITextureSRVCreateInfo& TextureSRVDesc, const TCHAR* TextureName)
	{
		return FRHITextureSRVCreateInfo::Validate(TextureDesc, TextureSRVDesc, TextureName, /* bFatal = */ true);
	}

protected:
	RHI_API static bool Validate(const FRHITextureDesc& TextureDesc, const FRHITextureSRVCreateInfo& TextureSRVDesc, const TCHAR* TextureName, bool bFatal);
};

struct FRHITextureUAVCreateInfo
{
public:
	FRHITextureUAVCreateInfo() = default;

	explicit FRHITextureUAVCreateInfo(uint8 InMipLevel, EPixelFormat InFormat = PF_Unknown, uint16 InFirstArraySlice = 0, uint16 InNumArraySlices = 0)
		: Format(InFormat)
		, MipLevel(InMipLevel)
		, FirstArraySlice(InFirstArraySlice)
		, NumArraySlices(InNumArraySlices)
	{}

	explicit FRHITextureUAVCreateInfo(ERHITextureMetaDataAccess InMetaData)
		: MetaData(InMetaData)
	{}

	FORCEINLINE bool operator==(const FRHITextureUAVCreateInfo& Other)const
	{
		return Format == Other.Format && MipLevel == Other.MipLevel && MetaData == Other.MetaData &&
                FirstArraySlice == Other.FirstArraySlice && NumArraySlices == Other.NumArraySlices &&
                DimensionOverride == Other.DimensionOverride;
	}

	FORCEINLINE bool operator!=(const FRHITextureUAVCreateInfo& Other)const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FRHITextureUAVCreateInfo& Info)
	{
		uint32 Hash = uint32(Info.Format) | uint32(Info.MipLevel) << 8 | uint32(Info.FirstArraySlice) << 16;
        Hash = HashCombine(Hash, Info.DimensionOverride.IsSet() ? uint32(*Info.DimensionOverride) : MAX_uint32);
		Hash = HashCombine(Hash, uint32(Info.NumArraySlices) | uint32(Info.MetaData) << 16);
		return Hash;
	}

	EPixelFormat Format = PF_Unknown;
	uint8 MipLevel = 0;
	uint16 FirstArraySlice = 0;
	uint16 NumArraySlices = 0;	// When 0, the default behavior will be used, e.g. all slices mapped.
	ERHITextureMetaDataAccess MetaData = ERHITextureMetaDataAccess::None;
    
    /** Specify a dimension to use which overrides the default  */
    TOptional<ETextureDimension> DimensionOverride;
};

/** Descriptor used to create a buffer resource */
struct FRHIBufferCreateInfo
{
	bool operator == (const FRHIBufferCreateInfo& Other) const
	{
		return (
			Size == Other.Size &&
			Stride == Other.Stride &&
			Usage == Other.Usage);
	}

	bool operator != (const FRHIBufferCreateInfo& Other) const
	{
		return !(*this == Other);
	}

	/** Total size of the buffer. */
	uint32 Size = 1;

	/** Stride in bytes */
	uint32 Stride = 1;

	/** Bitfields describing the uses of that buffer. */
	EBufferUsageFlags Usage = BUF_None;
};

struct FRHIBufferSRVCreateInfo
{
	explicit FRHIBufferSRVCreateInfo() = default;

	explicit FRHIBufferSRVCreateInfo(EPixelFormat InFormat)
		: Format(InFormat)
	{}

	FRHIBufferSRVCreateInfo(uint32 InStartOffsetBytes, uint32 InNumElements)
		: StartOffsetBytes(InStartOffsetBytes)
		, NumElements(InNumElements)
	{}

	FORCEINLINE bool operator==(const FRHIBufferSRVCreateInfo& Other)const
	{
		return Format == Other.Format
			&& StartOffsetBytes == Other.StartOffsetBytes
			&& NumElements == Other.NumElements;
	}

	FORCEINLINE bool operator!=(const FRHIBufferSRVCreateInfo& Other)const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FRHIBufferSRVCreateInfo& Desc)
	{
		return HashCombine(
			HashCombine(GetTypeHash(Desc.Format), GetTypeHash(Desc.StartOffsetBytes)),
			GetTypeHash(Desc.NumElements)
		);
	}

	/** Encoding format for the element. */
	EPixelFormat Format = PF_Unknown;

	/** Offset in bytes from the beginning of buffer */
	uint32 StartOffsetBytes = 0;

	/** Number of elements (whole buffer by default) */
	uint32 NumElements = UINT32_MAX;
};

struct FRHIBufferUAVCreateInfo
{
	FRHIBufferUAVCreateInfo() = default;

	explicit FRHIBufferUAVCreateInfo(EPixelFormat InFormat)
		: Format(InFormat)
	{}

	FORCEINLINE bool operator==(const FRHIBufferUAVCreateInfo& Other)const
	{
		return Format == Other.Format && bSupportsAtomicCounter == Other.bSupportsAtomicCounter && bSupportsAppendBuffer == Other.bSupportsAppendBuffer;
	}

	FORCEINLINE bool operator!=(const FRHIBufferUAVCreateInfo& Other)const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FRHIBufferUAVCreateInfo& Info)
	{
		return uint32(Info.Format) | uint32(Info.bSupportsAtomicCounter) << 8 | uint32(Info.bSupportsAppendBuffer) << 16;
	}

	/** Number of bytes per element (used for typed buffers). */
	EPixelFormat Format = PF_Unknown;

	/** Whether the uav supports atomic counter or append buffer operations (used for structured buffers) */
	bool bSupportsAtomicCounter = false;
	bool bSupportsAppendBuffer = false;
};

class FRHITextureViewCache
{
public:
	// Finds a UAV matching the descriptor in the cache or creates a new one and updates the cache.
	RHI_API FRHIUnorderedAccessView* GetOrCreateUAV(FRHICommandListBase& RHICmdList, FRHITexture* Texture, const FRHITextureUAVCreateInfo& CreateInfo);

	// Finds a SRV matching the descriptor in the cache or creates a new one and updates the cache.
	RHI_API FRHIShaderResourceView* GetOrCreateSRV(FRHICommandListBase& RHICmdList, FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo);

	UE_DEPRECATED(5.3, "GetOrCreateUAV now requires a command list.")
	RHI_API FRHIUnorderedAccessView* GetOrCreateUAV(FRHITexture* Texture, const FRHITextureUAVCreateInfo& CreateInfo);
	UE_DEPRECATED(5.3, "GetOrCreateSRV now requires a command list.")
	RHI_API FRHIShaderResourceView* GetOrCreateSRV(FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo);

	// Sets the debug name of the RHI view resources.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	RHI_API void SetDebugName(FRHICommandListBase& RHICmdList, const TCHAR* DebugName);
#else
	void SetDebugName(FRHICommandListBase& RHICmdList, const TCHAR* DebugName) {}
#endif

private:
	TArray<TPair<FRHITextureUAVCreateInfo, FUnorderedAccessViewRHIRef>, TInlineAllocator<1>> UAVs;
	TArray<TPair<FRHITextureSRVCreateInfo, FShaderResourceViewRHIRef>, TInlineAllocator<1>> SRVs;
};

class FRHIBufferViewCache
{
public:
	// Finds a UAV matching the descriptor in the cache or creates a new one and updates the cache.
	RHI_API FRHIUnorderedAccessView* GetOrCreateUAV(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, const FRHIBufferUAVCreateInfo& CreateInfo);

	// Finds a SRV matching the descriptor in the cache or creates a new one and updates the cache.
	RHI_API FRHIShaderResourceView* GetOrCreateSRV(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, const FRHIBufferSRVCreateInfo& CreateInfo);

	UE_DEPRECATED(5.3, "GetOrCreateUAV now requires a command list.")
	RHI_API FRHIUnorderedAccessView* GetOrCreateUAV(FRHIBuffer* Buffer, const FRHIBufferUAVCreateInfo& CreateInfo);
	UE_DEPRECATED(5.3, "GetOrCreateSRV now requires a command list.")
	RHI_API FRHIShaderResourceView* GetOrCreateSRV(FRHIBuffer* Buffer, const FRHIBufferSRVCreateInfo& CreateInfo);

	// Sets the debug name of the RHI view resources.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	RHI_API void SetDebugName(FRHICommandListBase& RHICmdList, const TCHAR* DebugName);
#else
	void SetDebugName(FRHICommandListBase& RHICmdList, const TCHAR* DebugName) {}
#endif

private:
	TArray<TPair<FRHIBufferUAVCreateInfo, FUnorderedAccessViewRHIRef>, TInlineAllocator<1>> UAVs;
	TArray<TPair<FRHIBufferSRVCreateInfo, FShaderResourceViewRHIRef>, TInlineAllocator<1>> SRVs;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Async/TaskGraphInterfaces.h"
#include "Containers/ClosableMpscQueue.h"
#include "Containers/ConsumeAllMpmcQueue.h"
#include "Containers/LockFreeList.h"
#include "Experimental/Containers/HazardPointer.h"
#include "Hash/CityHash.h"
#include "Misc/CoreDelegates.h"
#include "Misc/SecureHash.h"
#include "RHIShaderLibrary.h"
#include "RHITextureReference.h"
#include "TextureProfiler.h"
#endif

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "Serialization/MemoryImage.h"
#endif
