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
	int64 DedicatedVideoMemory;
	// -1 if unknown, in bytes
	int64 DedicatedSystemMemory;
	// -1 if unknown, in bytes
	int64 SharedSystemMemory;
	// Total amount of "graphics memory" that we think we can use for all our graphics resources, in bytes. -1 if unknown.
	int64 TotalGraphicsMemory;

	// Size of allocated memory, in bytes
	int64 AllocatedMemorySize;
	// Size of the largest memory fragment, in bytes
	int64 LargestContiguousAllocation;
	// 0 if streaming pool size limitation is disabled, in bytes
	int64 TexturePoolSize;
	// Upcoming adjustments to allocated memory, in bytes (async reallocations)
	int32 PendingMemoryAdjustment;

	// defaults
	FTextureMemoryStats()
		: DedicatedVideoMemory(-1)
		, DedicatedSystemMemory(-1)
		, SharedSystemMemory(-1)
		, TotalGraphicsMemory(-1)
		, AllocatedMemorySize(0)
		, LargestContiguousAllocation(0)
		, TexturePoolSize(0)
		, PendingMemoryAdjustment(0)
	{
	}

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
		return FMath::Max(TexturePoolSize - AllocatedMemorySize, (int64)0);
	}
};


// GPU stats

extern RHI_API int32 GNumDrawCallsRHI[MAX_NUM_GPUS];
extern RHI_API int32 GNumPrimitivesDrawnRHI[MAX_NUM_GPUS];

#if HAS_GPU_STATS

struct RHI_API FDrawCallCategoryName
{
	FDrawCallCategoryName();
	FDrawCallCategoryName(FName InName);

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

		FManager();
	};

	static FManager& GetManager();
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
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory 2D"), STAT_RenderTargetMemory2D, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory 3D"), STAT_RenderTargetMemory3D, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory Cube"), STAT_RenderTargetMemoryCube, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory 2D"), STAT_TextureMemory2D, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory 3D"), STAT_TextureMemory3D, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory Cube"), STAT_TextureMemoryCube, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Uniform buffer memory"), STAT_UniformBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Index buffer memory"), STAT_IndexBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Vertex buffer memory"), STAT_VertexBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Ray Tracing Acceleration Structure memory"), STAT_RTAccelerationStructureMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Structured buffer memory"), STAT_StructuredBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Pixel buffer memory"), STAT_PixelBufferMemory, STATGROUP_RHI, FPlatformMemory::MCR_GPU, RHI_API);
