// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright (C) Microsoft. All rights reserved.

/*=============================================================================
	GPUSkinCache.h: Performs skinning on a compute shader into a buffer to avoid vertex buffer skinning.
=============================================================================*/

// Requirements
// * Compute shader support (with Atomics)
// * Project settings needs to be enabled (r.SkinCache.CompileShaders)
// * feature need to be enabled (r.SkinCache.Mode)

// Features
// * Skeletal mesh, 4 / 8 weights per vertex, 16/32 index buffer
// * Supports Morph target animation (morph target blending is not done by this code)
// * Saves vertex shader computations when we render an object multiple times (EarlyZ, velocity, shadow, BasePass, CustomDepth, Shadow masking)
// * Fixes velocity rendering (needed for MotionBlur and TemporalAA) for WorldPosOffset animation and morph target animation
// * RecomputeTangents results in improved tangent space for WorldPosOffset animation and morph target animation
// * fixed amount of memory per Scene (r.SkinCache.SceneMemoryLimitInMB)
// * Velocity Rendering for MotionBlur and TemporalAA (test Velocity in BasePass)
// * r.SkinCache.Mode and r.SkinCache.RecomputeTangents can be toggled at runtime

// TODO:
// * Test: Tessellation
// * Quality/Optimization: increase TANGENT_RANGE for better quality or accumulate two components in one 32bit value
// * Bug: UpdateMorphVertexBuffer needs to handle SkinCacheObjects that have been rejected by the SkinCache (e.g. because it was running out of memory)
// * Refactor: Unify the 3 compute shaders to use the same C++ setup code for the variables
// * Optimization: Dispatch calls can be merged for better performance, stalls between Dispatch calls can be avoided (DX11 back door, DX12, console API)
// * Feature: Cloth is not supported yet (Morph targets is a similar code)
// * Feature: Support Static Meshes ?

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "RHIUtilities.h"
#include "RenderGraphDefinitions.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"
#include "GPUSkinPublicDefs.h"
#include "VertexFactory.h"
#include "CanvasTypes.h"
#include "CachedGeometry.h"
#include "DataDrivenShaderPlatformInfo.h"

class FGPUSkinPassthroughVertexFactory;
class FGPUBaseSkinVertexFactory;
class FMorphVertexBuffer;
class FSkeletalMeshLODRenderData;
class FSkeletalMeshObjectGPUSkin;
class FSkeletalMeshVertexClothBuffer;
class FVertexOffsetBuffers;
struct FClothSimulData;
struct FSkelMeshRenderSection;
struct FVertexBufferAndSRV;
struct FRayTracingGeometrySegment;
struct FSkinBatchVertexFactoryUserData;

extern bool ShouldWeCompileGPUSkinVFShaders(EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel);

extern ENGINE_API bool GPUSkinCacheNeedsDuplicatedVertices();

// Is it actually enabled?
extern ENGINE_API int32 GEnableGPUSkinCache;
extern int32 GSkinCacheRecomputeTangents;

class FGPUSkinCacheEntry;

struct FClothSimulEntry
{
	FVector3f Position;
	FVector3f Normal;

	/**
	 * Serializer
	 *
	 * @param Ar - archive to serialize with
	 * @param V - vertex to serialize
	 * @return archive that was used
	 */
	friend FArchive& operator<<(FArchive& Ar, FClothSimulEntry& V)
	{
		Ar << V.Position
		   << V.Normal;
		return Ar;
	}
};

enum class EGPUSkinCacheEntryMode
{
	Raster,
	RayTracing
};

class FGPUSkinCache
{
public:
	struct FRWBufferTracker;

	enum ESkinCacheInitSettings
	{
		// max 256 bones as we use a byte to index
		MaxUniformBufferBones = 256,
		// Controls the output format on GpuSkinCacheComputeShader.usf
		RWTangentXOffsetInFloats = 0,	// Packed U8x4N
		RWTangentZOffsetInFloats = 1,	// Packed U8x4N

		// 3 ints for normal, 3 ints for tangent, 1 for orientation = 7, rounded up to 8 as it should result in faster math and caching
		IntermediateAccumBufferNumInts = 8,
	};

	struct FDispatchEntry
	{
		FGPUSkinCacheEntry* SkinCacheEntry = nullptr;
		uint32 Section = 0;	
	};

	struct FSortedDispatchEntry
	{
		int32 ShaderIndex;
		int32 BatchIndex;
	};

	FGPUSkinCache() = delete;
	ENGINE_API FGPUSkinCache(ERHIFeatureLevel::Type InFeatureLevel, bool bInRequiresMemoryLimit, UWorld* InWorld);
	ENGINE_API ~FGPUSkinCache();

	static void UpdateSkinWeightBuffer(FGPUSkinCacheEntry* Entry);
	static void SetEntryGPUSkin(FGPUSkinCacheEntry* Entry, FSkeletalMeshObjectGPUSkin* Skin);

	bool ProcessEntry(
		EGPUSkinCacheEntryMode Mode,
		FRHICommandList& RHICmdList, 
		FGPUBaseSkinVertexFactory* VertexFactory,
		FGPUSkinPassthroughVertexFactory* TargetVertexFactory, 
		const FSkelMeshRenderSection& BatchElement, 
		FSkeletalMeshObjectGPUSkin* Skin,
		const FMorphVertexBuffer* MorphVertexBuffer, 
		const FSkeletalMeshVertexClothBuffer* ClothVertexBuffer, 
		const FClothSimulData* SimData,
		const FMatrix44f& ClothToLocal,
		float ClothBlendWeight, 
		FVector3f ClothScale,
		uint32 RevisionNumber, 
		int32 Section,
		int32 LOD,
		bool bRecreating,
		FGPUSkinCacheEntry*& InOutEntry
		);

	static void GetShaderVertexStreams(
		const FGPUSkinCacheEntry* Entry,
		int32 Section,
		const FGPUSkinPassthroughVertexFactory* VertexFactory,
		FVertexInputStreamArray& VertexStreams);

	static void Release(FGPUSkinCacheEntry*& SkinCacheEntry);

	static const FSkinBatchVertexFactoryUserData* GetVertexFactoryUserData(FGPUSkinCacheEntry* Entry, int32 Section);

	static bool IsEntryValid(FGPUSkinCacheEntry* SkinCacheEntry, int32 Section);
	static FColor GetVisualizationDebugColor(const FName& GPUSkinCacheVisualizationMode, FGPUSkinCacheEntry* Entry, FGPUSkinCacheEntry* RayTracingEntry, uint32 SectionIndex);
	ENGINE_API void DrawVisualizationInfoText(const FName& GPUSkinCacheVisualizationMode, FScreenMessageWriter& ScreenMessageWriter) const;

	ENGINE_API uint64 GetExtraRequiredMemoryAndReset();

	static bool IsGPUSkinCacheRayTracingSupported();

	enum
	{
		NUM_BUFFERS = 2,
	};

	struct FSkinCacheRWBuffer
	{
		FRWBuffer	Buffer;
		ERHIAccess	AccessState = ERHIAccess::Unknown;	// Keep track of current access state
		// See note in FGPUSkinCache::GetBufferUAVs()
		mutable uint32	UniqueOpToken = 0;

		void Release()
		{
			Buffer.Release();
			AccessState = ERHIAccess::Unknown;
		}

		// Update the access state and return transition info
		FRHITransitionInfo UpdateAccessState(ERHIAccess NewState)
		{
			ERHIAccess OldState = AccessState;
			AccessState = NewState;
			return FRHITransitionInfo(Buffer.UAV.GetReference(), OldState, AccessState);
		}
	};

	struct FRWBuffersAllocation
	{
		friend struct FRWBufferTracker;

		FRWBuffersAllocation(uint32 InNumVertices, bool InWithTangents, bool InUseIntermediateTangents, uint32 InIntermediateAccumulatedTangentsSize, FRHICommandList& RHICmdList, const FName& OwnerName)
			: NumVertices(InNumVertices), WithTangents(InWithTangents), UseIntermediateTangents(InUseIntermediateTangents), IntermediateAccumulatedTangentsSize(InIntermediateAccumulatedTangentsSize)
		{
			const static FLazyName PositionsName(TEXT("SkinCachePositions"));
			const static FLazyName TangentsName(TEXT("SkinCacheTangents"));
			const static FLazyName IntermediateTangentsName(TEXT("SkinCacheIntermediateTangents"));
			const static FLazyName IntermediateAccumulatedTangentsName(TEXT("SkinCacheIntermediateAccumulatedTangents"));

			for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
			{
				PositionBuffers[Index].Buffer.ClassName = PositionsName;
				PositionBuffers[Index].Buffer.OwnerName = OwnerName;
				PositionBuffers[Index].Buffer.Initialize(RHICmdList, TEXT("SkinCachePositions"), PosBufferBytesPerElement, NumVertices * 3, PF_R32_FLOAT, BUF_Static);
				PositionBuffers[Index].Buffer.Buffer->SetOwnerName(OwnerName);
				PositionBuffers[Index].AccessState = ERHIAccess::Unknown;
			}
			if (WithTangents)
			{
				// OpenGL ES does not support writing to RGBA16_SNORM images, instead pack data into SINT in the shader
				const EPixelFormat TangentsFormat = IsOpenGLPlatform(GMaxRHIShaderPlatform) ? PF_R16G16B16A16_SINT : PF_R16G16B16A16_SNORM;
				
				Tangents.Buffer.ClassName = TangentsName;
				Tangents.Buffer.OwnerName = OwnerName;
				Tangents.Buffer.Initialize(RHICmdList, TEXT("SkinCacheTangents"), TangentBufferBytesPerElement, NumVertices * 2, TangentsFormat, BUF_Static);
				Tangents.Buffer.Buffer->SetOwnerName(OwnerName);
				Tangents.AccessState = ERHIAccess::Unknown;
				if (UseIntermediateTangents)
				{
					IntermediateTangents.Buffer.ClassName = IntermediateTangentsName;
					IntermediateTangents.Buffer.OwnerName = OwnerName;
					IntermediateTangents.Buffer.Initialize(RHICmdList, TEXT("SkinCacheIntermediateTangents"), TangentBufferBytesPerElement, NumVertices * 2, TangentsFormat, BUF_Static);
					IntermediateTangents.Buffer.Buffer->SetOwnerName(OwnerName);
					IntermediateTangents.AccessState = ERHIAccess::Unknown;
				}
			}
			if (IntermediateAccumulatedTangentsSize > 0)
			{
				IntermediateAccumulatedTangents.Buffer.ClassName = IntermediateAccumulatedTangentsName;
				IntermediateAccumulatedTangents.Buffer.OwnerName = OwnerName;
				IntermediateAccumulatedTangents.Buffer.Initialize(RHICmdList, TEXT("SkinCacheIntermediateAccumulatedTangents"), sizeof(int32), IntermediateAccumulatedTangentsSize * FGPUSkinCache::IntermediateAccumBufferNumInts, PF_R32_SINT, BUF_UnorderedAccess);
				IntermediateAccumulatedTangents.Buffer.Buffer->SetOwnerName(OwnerName);
				IntermediateAccumulatedTangents.AccessState = ERHIAccess::Unknown;
				// The UAV must be zero-filled. We leave it zeroed after each round (see RecomputeTangentsPerVertexPass.usf), so this is only needed on when the buffer is first created.
				RHICmdList.ClearUAVUint(IntermediateAccumulatedTangents.Buffer.UAV, FUintVector4(0, 0, 0, 0));
			}
		}

		~FRWBuffersAllocation()
		{
			for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
			{
				PositionBuffers[Index].Release();
			}
			if (WithTangents)
			{
				Tangents.Release();
				IntermediateTangents.Release();
			}
			if (IntermediateAccumulatedTangentsSize > 0)
			{
				IntermediateAccumulatedTangents.Release();
			}
		}

		static uint64 CalculateRequiredMemory(uint32 InNumVertices, bool InWithTangents, bool InUseIntermediateTangents, uint32 InIntermediateAccumulatedTangentsSize)
		{
			uint64 PositionBufferSize = PosBufferBytesPerElement * InNumVertices * 3 * NUM_BUFFERS;
			uint64 TangentBufferSize = InWithTangents ? TangentBufferBytesPerElement * InNumVertices * 2 : 0;
			uint64 IntermediateTangentBufferSize = 0;
			if (InUseIntermediateTangents)
			{
				IntermediateTangentBufferSize = InWithTangents ? TangentBufferBytesPerElement * InNumVertices * 2 : 0;
			}
			uint64 AccumulatedTangentBufferSize = InIntermediateAccumulatedTangentsSize * FGPUSkinCache::IntermediateAccumBufferNumInts * sizeof(int32);
			return TangentBufferSize + IntermediateTangentBufferSize + PositionBufferSize + AccumulatedTangentBufferSize;
		}

		uint64 GetNumBytes() const
		{
			return CalculateRequiredMemory(NumVertices, WithTangents, UseIntermediateTangents, IntermediateAccumulatedTangentsSize);
		}

		FSkinCacheRWBuffer* GetTangentBuffer()
		{
			return WithTangents ? &Tangents : nullptr;
		}

		FSkinCacheRWBuffer* GetIntermediateTangentBuffer()
		{
			return (WithTangents && UseIntermediateTangents) ? &IntermediateTangents : nullptr;
		}

		FSkinCacheRWBuffer* GetIntermediateAccumulatedTangentBuffer()
		{
			return IntermediateAccumulatedTangentsSize > 0 ? &IntermediateAccumulatedTangents : nullptr;
		}

		void RemoveAllFromTransitionArray(TSet<FSkinCacheRWBuffer*>& BuffersToTransition);

	private:
		// Output of the GPU skinning (ie Pos, Normals)
		FSkinCacheRWBuffer PositionBuffers[NUM_BUFFERS];

		FSkinCacheRWBuffer Tangents;
		FSkinCacheRWBuffer IntermediateTangents;
		FSkinCacheRWBuffer IntermediateAccumulatedTangents;	// Intermediate buffer used to accumulate results of triangle pass to be passed onto vertex pass

		const uint32 NumVertices;
		const bool WithTangents;
		const bool UseIntermediateTangents;
		const uint32 IntermediateAccumulatedTangentsSize;

		static const uint32 PosBufferBytesPerElement = 4;
		static const uint32 TangentBufferBytesPerElement = 8;
	};

	struct FRWBufferTracker
	{
		FRWBuffersAllocation* Allocation;

		FRWBufferTracker()
			: Allocation(nullptr)
		{
			Reset();
		}

		void Reset()
		{
			for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
			{
				Revisions[Index] = 0;
				BoneBuffers[Index] = nullptr;
			}
		}

		inline uint32 GetNumBytes() const
		{
			return IntCastChecked<uint32>(Allocation->GetNumBytes());
		}

		FSkinCacheRWBuffer* Find(const FVertexBufferAndSRV& BoneBuffer, uint32 Revision)
		{
			for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
			{
				if (Revisions[Index] == Revision && BoneBuffers[Index] == &BoneBuffer)
				{
					return &Allocation->PositionBuffers[Index];
				}
			}

			return nullptr;
		}

		FSkinCacheRWBuffer* GetTangentBuffer()
		{
			return Allocation ? Allocation->GetTangentBuffer() : nullptr;
		}

		FSkinCacheRWBuffer* GetIntermediateTangentBuffer()
		{
			return Allocation ? Allocation->GetIntermediateTangentBuffer() : nullptr;
		}

		FSkinCacheRWBuffer* GetIntermediateAccumulatedTangentBuffer()
		{
			return Allocation ? Allocation->GetIntermediateAccumulatedTangentBuffer() : nullptr;
		}

		FSkinCacheRWBuffer* Advance(const FVertexBufferAndSRV& BoneBuffer1, uint32 Revision1, const FVertexBufferAndSRV& BoneBuffer2, uint32 Revision2)
		{
			FSkinCacheRWBuffer* Result = nullptr;
			const FVertexBufferAndSRV* InBoneBuffers[2] = { &BoneBuffer1 , &BoneBuffer2 };
			uint32 InRevisions[2] = { Revision1 , Revision2 };

			for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
			{
				bool Needed = false;
				for (int32 i = 0; i < 2; ++i)
				{
					if (Revisions[Index] == InRevisions[i] && BoneBuffers[Index] == InBoneBuffers[i])
					{
						if (i == 0)
						{
							Result = &Allocation->PositionBuffers[Index];
						}
						Needed = true;
					}
				}

				if (!Needed)
				{
					Revisions[Index] = Revision1;
					BoneBuffers[Index] = &BoneBuffer1;
					Result = &Allocation->PositionBuffers[Index];
					break;
				}
			}
			return Result;
		}

		// On recreate of the render state where the GPU skin cache entry is preserved, the bone buffer will have been reallocated,
		// even though the transforms didn't change.  We need to force the Find() call above to treat the data as up-to-date, which
		// can be accomplished by updating the BoneBuffer pointer for the previous Revision, so it matches again.
		void UpdatePreviousBoneBuffer(const FVertexBufferAndSRV& PreviousBoneBuffer, uint32 PreviousRevision)
		{
			for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
			{
				if (Revisions[Index] == PreviousRevision)
				{
					BoneBuffers[Index] = &PreviousBoneBuffer;
					break;
				}
			}
		}

	private:
		uint32 Revisions[NUM_BUFFERS];
		const FVertexBufferAndSRV* BoneBuffers[NUM_BUFFERS];
	};

	FGPUSkinCacheEntry const* GetSkinCacheEntry(uint32 ComponentId) const;
	static FRWBuffer* GetPositionBuffer(FGPUSkinCacheEntry const* Entry, uint32 SectionIndex);
	static FRWBuffer* GetPreviousPositionBuffer(FGPUSkinCacheEntry const* Entry, uint32 SectionIndex);
	static FRWBuffer* GetTangentBuffer(FGPUSkinCacheEntry const* Entry, uint32 SectionIndex);
	static uint32 GetUpdatedFrame(FGPUSkinCacheEntry const* Entry, uint32 SectionIndex);

	// Deprecated function. Can remove include of CachedGeometry.h when this is removed.
	UE_DEPRECATED(5.1, "Use GetPositionBuffer() or similar instead.")
	FCachedGeometry::Section GetCachedGeometry(FGPUSkinCacheEntry* InOutEntry, uint32 SectionId);

#if RHI_RAYTRACING
	void ProcessRayTracingGeometryToUpdate(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* SkinCacheEntry);
#endif // RHI_RAYTRACING

	void BeginBatchDispatch();
	void EndBatchDispatch();
	void ENGINE_API DoDispatch(FRHICommandList& RHICmdList);

	inline ERHIFeatureLevel::Type GetFeatureLevel() const { return FeatureLevel; }

	inline bool HasWork() const { return !BatchDispatches.IsEmpty(); }

protected:
	void MakeBufferTransitions(FRHICommandList& RHICmdList, TArray<FSkinCacheRWBuffer*>& Buffers, ERHIAccess ToState);
	void GetBufferUAVs(const TArray<FSkinCacheRWBuffer*>& InBuffers, TArray<FRHIUnorderedAccessView*>& OutUAVs);

	TArray<FRWBuffersAllocation*> Allocations;
	TArray<FGPUSkinCacheEntry*> Entries;
	TSet<FGPUSkinCacheEntry*> PendingProcessRTGeometryEntries;
	TArray<FDispatchEntry> BatchDispatches;

	FRWBuffersAllocation* TryAllocBuffer(uint32 NumVertices, bool WithTangnents, bool UseIntermediateTangents, uint32 NumTriangles, FRHICommandList& RHICmdList, const FName& OwnerName);
	void DoDispatch(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* SkinCacheEntry, int32 Section, int32 RevisionNumber);
	void DispatchUpdateSkinTangents(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* Entry, int32 SectionIndex, FSkinCacheRWBuffer*& StagingBuffer, bool bTrianglePass);

	void PrepareUpdateSkinning(
		FGPUSkinCacheEntry* Entry, 
		int32 Section, 
		uint32 RevisionNumber, 
		TArray<FSkinCacheRWBuffer*>* OverlappedUAVs
		);

	void DispatchUpdateSkinning(
		FRHICommandList& RHICmdList, 
		FGPUSkinCacheEntry* Entry, 
		int32 Section, 
		uint32 RevisionNumber,
		TArray<FSkinCacheRWBuffer*>& BuffersToTransitionToRead
		);

	void Cleanup();
	static void TransitionAllToReadable(FRHICommandList& RHICmdList, const TArray<FSkinCacheRWBuffer*>& BuffersToTransitionToRead);
	static void ReleaseSkinCacheEntry(FGPUSkinCacheEntry* SkinCacheEntry);
	void InvalidateAllEntries();
	uint64 UsedMemoryInBytes;
	uint64 ExtraRequiredMemory;
	int32 FlushCounter;
	bool bRequiresMemoryLimit;
	bool bShouldBatchDispatches = false;

	// For recompute tangents, holds the data required between compute shaders
	TArray<FSkinCacheRWBuffer> StagingBuffers;
	int32 CurrentStagingBufferIndex;

	ERHIFeatureLevel::Type FeatureLevel;
	UWorld* World;

	static void CVarSinkFunction();
	static FAutoConsoleVariableSink CVarSink;

	void IncrementDispatchCounter(FRHICommandList& RHICmdList);
	int32 DispatchCounter = 0;

	void PrintMemorySummary() const;
	FString GetSkeletalMeshObjectName(const FSkeletalMeshObjectGPUSkin* GPUSkin) const;
};

DECLARE_STATS_GROUP(TEXT("GPU Skin Cache"), STATGROUP_GPUSkinCache, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Sections Skinned"), STAT_GPUSkinCache_TotalNumChunks, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Vertices Skinned"), STAT_GPUSkinCache_TotalNumVertices, STATGROUP_GPUSkinCache,);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Total Memory Bytes Used"), STAT_GPUSkinCache_TotalMemUsed, STATGROUP_GPUSkinCache, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Intermediate buffer for Recompute Tangents"), STAT_GPUSkinCache_TangentsIntermediateMemUsed, STATGROUP_GPUSkinCache, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Triangles for Recompute Tangents"), STAT_GPUSkinCache_NumTrianglesForRecomputeTangents, STATGROUP_GPUSkinCache, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Sections Processed"), STAT_GPUSkinCache_NumSectionsProcessed, STATGROUP_GPUSkinCache, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num SetVertexStreams"), STAT_GPUSkinCache_NumSetVertexStreams, STATGROUP_GPUSkinCache, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num PreGDME"), STAT_GPUSkinCache_NumPreGDME, STATGROUP_GPUSkinCache, );
