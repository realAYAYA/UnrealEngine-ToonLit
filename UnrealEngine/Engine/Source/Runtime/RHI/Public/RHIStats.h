// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StaticArray.h"
#include "MultiGPU.h"
#include "RHIDefinitions.h"
#include "Stats/Stats.h"

struct FTextureMemoryStats
{
	// Hardware state (never change after device creation):

	// -1 if unknown, in bytes
	int64 DedicatedVideoMemory = -1;

	// -1 if unknown, in bytes
	int64 DedicatedSystemMemory = -1;

	// -1 if unknown, in bytes
	int64 SharedSystemMemory = -1;

	// Total amount of "graphics memory" that we think we can use for all our graphics resources, in bytes. -1 if unknown.
	int64 TotalGraphicsMemory = -1;

	// Size of allocated memory, in bytes
	UE_DEPRECATED(5.3, "AllocatedMemorySize was too vague, use StreamingMemorySize in its place")
	int64 AllocatedMemorySize = 0;

	// Size of memory allocated to streaming textures, in bytes
	uint64 StreamingMemorySize = 0;

	// Size of memory allocated to non-streaming textures, in bytes
	uint64 NonStreamingMemorySize = 0;

	// Size of the largest memory fragment, in bytes
	int64 LargestContiguousAllocation = 0;
	
	// 0 if streaming pool size limitation is disabled, in bytes
	int64 TexturePoolSize = 0;

	// Upcoming adjustments to allocated memory, in bytes (async reallocations)
	UE_DEPRECATED(5.3, "PendingMemoryAdjustment is unused")
	int32 PendingMemoryAdjustment = 0;

	bool AreHardwareStatsValid() const
	{
		// pardon the redundancy, have a broken compiler (__EMSCRIPTEN__) that needs these types spelled out...
		return ((int64)DedicatedVideoMemory >= 0 && (int64)DedicatedSystemMemory >= 0 && (int64)SharedSystemMemory >= 0);
	}

	bool IsUsingLimitedPoolSize() const
	{
		return TexturePoolSize > 0;
	}

	int64 ComputeAvailableMemorySize() const
	{
		return FMath::Max<int64>(TexturePoolSize - StreamingMemorySize, 0);
	}
};


// GPU stats

extern RHI_API int32 GNumDrawCallsRHI[MAX_NUM_GPUS];
extern RHI_API int32 GNumPrimitivesDrawnRHI[MAX_NUM_GPUS];

#if HAS_GPU_STATS

struct FDrawCallCategoryName
{
	RHI_API FDrawCallCategoryName();
	RHI_API FDrawCallCategoryName(FName InName);

	bool ShouldCountDraws() const { return Index != -1; }

	FName  const Name;
	uint32 const Index;

	static constexpr int32 MAX_DRAWCALL_CATEGORY = 256;

	struct FManager
	{
		TStaticArray<FDrawCallCategoryName*, MAX_DRAWCALL_CATEGORY> Array;

		// A backup of the counts that can be used to display on screen to avoid flickering.
		TStaticArray<TStaticArray<int32, MAX_NUM_GPUS>, MAX_DRAWCALL_CATEGORY> DisplayCounts;

		int32 NumCategory;

		RHI_API FManager();
	};

	RHI_API static FManager& GetManager();
};

// RHI counter stats.
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("DrawPrimitive calls"), STAT_RHIDrawPrimitiveCalls, STATGROUP_RHI, RHI_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Triangles drawn"), STAT_RHITriangles, STATGROUP_RHI, RHI_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Lines drawn"), STAT_RHILines, STATGROUP_RHI, RHI_API);

#else

struct FDrawCallCategoryName {};

#endif

// Macros for use inside RHI context Draw functions.
// Updates the Stats structure on the underlying RHI context class (IRHICommandContext)

#define RHI_DRAW_CALL_INC() do { Stats->Draws++; } while (false)
#define RHI_DRAW_CALL_STATS(PrimitiveType,NumPrimitives)                  \
	do                                                                    \
	{												                      \
		switch (PrimitiveType)                                            \
		{                                                                 \
		case PT_TriangleList : Stats->Triangles  += NumPrimitives; break; \
		case PT_TriangleStrip: Stats->Triangles  += NumPrimitives; break; \
		case PT_LineList     : Stats->Lines      += NumPrimitives; break; \
		case PT_QuadList     : Stats->Quads      += NumPrimitives; break; \
		case PT_PointList    : Stats->Points     += NumPrimitives; break; \
		case PT_RectList     : Stats->Rectangles += NumPrimitives; break; \
		}                                                                 \
		Stats->Draws++;                                                   \
	} while(false)

struct FRHIPerCategoryDrawStats
{
	uint32 Draws;
	uint32 Triangles;
	uint32 Lines;
	uint32 Quads;
	uint32 Points;
	uint32 Rectangles;

	uint32 GetTotalPrimitives() const
	{
		return Triangles
			+ Lines
			+ Quads
			+ Points
			+ Rectangles;
	}

	FRHIPerCategoryDrawStats& operator += (FRHIPerCategoryDrawStats const& RHS)
	{
		Draws += RHS.Draws;
		Triangles += RHS.Triangles;
		Lines += RHS.Lines;
		Quads += RHS.Quads;
		Points += RHS.Points;
		Rectangles += RHS.Rectangles;
		return *this;
	}
};

struct FRHIDrawStats
{
#if HAS_GPU_STATS
	// The +1 is for "uncategorised"
	static constexpr int32 NumCategories = FDrawCallCategoryName::MAX_DRAWCALL_CATEGORY + 1;
#else
	static constexpr int32 NumCategories = 1;
#endif

	static constexpr int32 NoCategory = NumCategories - 1;

	using FPerCategoryStats = FRHIPerCategoryDrawStats;

	struct FPerGPUStats
	{
		FPerCategoryStats& GetCategory(uint32 Category)
		{
			checkSlow(Category < UE_ARRAY_COUNT(Categories));
			return Categories[Category];
		}

	private:
		FPerCategoryStats Categories[NumCategories];
	};

	FPerGPUStats& GetGPU(uint32 GPUIndex)
	{
		checkSlow(GPUIndex < UE_ARRAY_COUNT(GPUs));
		return GPUs[GPUIndex];
	}

	FRHIDrawStats()
	{
		Reset();
	}

	void Reset()
	{
		FMemory::Memzero(*this);
	}

	RHI_API void Accumulate(FRHIDrawStats& RHS);

private:
	FPerGPUStats GPUs[MAX_NUM_GPUS];
};

// RHI memory stats.
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render Target 2D Memory"), STAT_RenderTargetMemory2D, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render Target 3D Memory"), STAT_RenderTargetMemory3D, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render Target Cube Memory"), STAT_RenderTargetMemoryCube, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);

DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("UAV Texture Memory"), STAT_UAVTextureMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);

DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture 2D Memory"), STAT_TextureMemory2D, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture 3D Memory"), STAT_TextureMemory3D, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture Cube Memory"), STAT_TextureMemoryCube, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);

DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Uniform Buffer Memory"), STAT_UniformBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Index Buffer Memory"), STAT_IndexBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Vertex Buffer Memory"), STAT_VertexBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("RayTracing Acceleration Structure Memory"), STAT_RTAccelerationStructureMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Structured Buffer Memory"), STAT_StructuredBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Byte Address Buffer Memory"), STAT_ByteAddressBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Draw Indirect Buffer Memory"), STAT_DrawIndirectBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Misc Buffer Memory"), STAT_MiscBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Sampler Descriptors Allocated"), STAT_SamplerDescriptorsAllocated, STATGROUP_RHI, RHI_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Resource Descriptors Allocated"), STAT_ResourceDescriptorsAllocated, STATGROUP_RHI, RHI_API);

DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Bindless Sampler Heap"), STAT_BindlessSamplerHeapMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Bindless Resource Heap"), STAT_BindlessResourceHeapMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Bindless Sampler Descriptors Allocated"), STAT_BindlessSamplerDescriptorsAllocated, STATGROUP_RHI, RHI_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Bindless Resource Descriptors Allocated"), STAT_BindlessResourceDescriptorsAllocated, STATGROUP_RHI, RHI_API);
