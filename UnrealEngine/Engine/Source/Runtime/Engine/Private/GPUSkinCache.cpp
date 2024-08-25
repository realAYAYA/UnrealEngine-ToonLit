// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright (C) Microsoft. All rights reserved.

/*=============================================================================
GPUSkinCache.cpp: Performs skinning on a compute shader into a buffer to avoid vertex buffer skinning.
=============================================================================*/

#include "GPUSkinCache.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/RenderCommandPipes.h"
#include "SkeletalRenderGPUSkin.h"
#include "MeshDrawShaderBindings.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"
#include "RenderCaptureInterface.h"
#include "Engine/SkinnedAssetCommon.h"
#include "GPUSkinCacheVisualizationData.h"
#include "RHIContext.h"
#include "ShaderPlatformCachedIniValue.h"
#include "RenderUtils.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "Stats/StatsTrace.h"
#include "UObject/UObjectIterator.h"
#include "Algo/Sort.h"

DEFINE_STAT(STAT_GPUSkinCache_TotalNumChunks);
DEFINE_STAT(STAT_GPUSkinCache_TotalNumVertices);
DEFINE_STAT(STAT_GPUSkinCache_TotalMemUsed);
DEFINE_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed);
DEFINE_STAT(STAT_GPUSkinCache_NumTrianglesForRecomputeTangents);
DEFINE_STAT(STAT_GPUSkinCache_NumSectionsProcessed);
DEFINE_STAT(STAT_GPUSkinCache_NumSetVertexStreams);
DEFINE_STAT(STAT_GPUSkinCache_NumPreGDME);
DEFINE_LOG_CATEGORY_STATIC(LogSkinCache, Log, All);

/** Exec helper to handle GPU Skin Cache related commands. */
class FSkinCacheExecHelper : public FSelfRegisteringExec
{
	virtual bool Exec_Runtime(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		/** Command to list all skeletal mesh lods which have the skin cache disabled. */
		if (FParse::Command(&Cmd, TEXT("list skincacheusage")))
		{
			UE_LOG(LogTemp, Display, TEXT("Name, Lod Index, Skin Cache Usage"));

			for (TObjectIterator<USkeletalMesh> It; It; ++It)
			{
				if (USkeletalMesh* SkeletalMesh = *It)
				{
					for (int32 LODIndex = 0; LODIndex < SkeletalMesh->GetLODNum(); ++LODIndex)
					{
						if (FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex))
						{
							UE_LOG(LogTemp, Display, TEXT("%s, %d, %d"), *SkeletalMesh->GetFullName(), LODIndex, int(LODInfo->SkinCacheUsage));
						}
					}
				}
			}
			return true;
		}
		return false;
	}
};
static FSkinCacheExecHelper GSkelMeshExecHelper;

static int32 GEnableGPUSkinCacheShaders = 0;

static TAutoConsoleVariable<bool> CVarAllowGPUSkinCache(
	TEXT("r.SkinCache.Allow"),
	true,
	TEXT("Whether or not to allow the GPU skin Cache system to be enabled.\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static FAutoConsoleVariableRef CVarEnableGPUSkinCacheShaders(
	TEXT("r.SkinCache.CompileShaders"),
	GEnableGPUSkinCacheShaders,
	TEXT("Whether or not to compile the GPU compute skinning cache shaders.\n")
	TEXT("This will compile the shaders for skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("GPUSkinVertexFactory.usf needs to be touched to cause a recompile if this changes.\n")
	TEXT("0 is off(default), 1 is on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static TAutoConsoleVariable<bool> CVarSkipCompilingGPUSkinVF(
	TEXT("r.SkinCache.SkipCompilingGPUSkinVF"),
	false,
	TEXT("Reduce GPU Skin Vertex Factory shader permutations. Cannot be disabled while the skin cache is turned off.\n")
	TEXT(" False ( 0): Compile all GPU Skin Vertex factory variants.\n")
	TEXT(" True  ( 1): Don't compile all GPU Skin Vertex factory variants."),
    ECVF_RenderThreadSafe | ECVF_ReadOnly
);

// 0/1
int32 GEnableGPUSkinCache = 1;
static TAutoConsoleVariable<int32> CVarEnableGPUSkinCache(
	TEXT("r.SkinCache.Mode"),
	1,
	TEXT("Whether or not to use the GPU compute skinning cache.\n")
	TEXT("This will perform skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("Requires r.SkinCache.CompileShaders=1 and r.SkinCache.Allow=1\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on(default)\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarDefaultGPUSkinCacheBehavior(
	TEXT("r.SkinCache.DefaultBehavior"),
	(int32)ESkinCacheDefaultBehavior::Inclusive,
	TEXT("Default behavior if all skeletal meshes are included/excluded from the skin cache. If Support Ray Tracing is enabled on a mesh, will force inclusive behavior on that mesh.\n")
	TEXT(" Exclusive ( 0): All skeletal meshes are excluded from the skin cache. Each must opt in individually.\n")
	TEXT(" Inclusive ( 1): All skeletal meshes are included into the skin cache. Each must opt out individually. (default)")
	);

int32 GSkinCacheRecomputeTangents = 2;
TAutoConsoleVariable<int32> CVarGPUSkinCacheRecomputeTangents(
	TEXT("r.SkinCache.RecomputeTangents"),
	2,
	TEXT("This option enables recomputing the vertex tangents on the GPU.\n")
	TEXT("Can be changed at runtime, requires both r.SkinCache.CompileShaders=1, r.SkinCache.Mode=1 and r.SkinCache.Allow=1\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on, forces all skinned object to Recompute Tangents\n")
	TEXT(" 2: on, only recompute tangents on skinned objects who ticked the Recompute Tangents checkbox(default)\n"),
	ECVF_RenderThreadSafe
);

static int32 GNumTangentIntermediateBuffers = 1;
static TAutoConsoleVariable<float> CVarGPUSkinNumTangentIntermediateBuffers(
	TEXT("r.SkinCache.NumTangentIntermediateBuffers"),
	1,
	TEXT("How many intermediate buffers to use for intermediate results while\n")
	TEXT("doing Recompute Tangents; more may allow the GPU to overlap compute jobs."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarGPUSkinCacheDebug(
	TEXT("r.SkinCache.Debug"),
	1.0f,
	TEXT("A scaling constant passed to the SkinCache shader, useful for debugging"),
	ECVF_RenderThreadSafe
);

static float GSkinCacheSceneMemoryLimitInMB = 128.0f;
static TAutoConsoleVariable<float> CVarGPUSkinCacheSceneMemoryLimitInMB(
	TEXT("r.SkinCache.SceneMemoryLimitInMB"),
	128.0f,
	TEXT("Maximum memory allowed to be allocated per World/Scene in Megs"),
	ECVF_RenderThreadSafe
);

static int32 GAllowDupedVertsForRecomputeTangents = 0;
FAutoConsoleVariableRef CVarGPUSkinCacheAllowDupedVertesForRecomputeTangents(
	TEXT("r.SkinCache.AllowDupedVertsForRecomputeTangents"),
	GAllowDupedVertsForRecomputeTangents,
	TEXT("0: off (default)\n")
	TEXT("1: Forces that vertices at the same position will be treated differently and has the potential to cause seams when verts are split.\n"),
	ECVF_RenderThreadSafe
);

int32 GRecomputeTangentsParallelDispatch = 0;
FAutoConsoleVariableRef CVarRecomputeTangentsParallelDispatch(
	TEXT("r.SkinCache.RecomputeTangentsParallelDispatch"),
	GRecomputeTangentsParallelDispatch,
	TEXT("This option enables parallel dispatches for recompute tangents.\n")
	TEXT(" 0: off (default), triangle pass is interleaved with vertex pass, requires resource barriers in between. \n")
	TEXT(" 1: on, batch triangle passes together, resource barrier, followed by vertex passes together, cost more memory. \n"),
	ECVF_RenderThreadSafe
);

static int32 GSkinCacheMaxDispatchesPerCmdList = 0;
FAutoConsoleVariableRef CVarGPUSkinCacheMaxDispatchesPerCmdList(
	TEXT("r.SkinCache.MaxDispatchesPerCmdList"),
	GSkinCacheMaxDispatchesPerCmdList,
	TEXT("Maximum number of compute shader dispatches which are batched together into a single command list to fix potential TDRs."),
	ECVF_RenderThreadSafe
);

static int32 GSkinCachePrintMemorySummary = 0;
FAutoConsoleVariableRef CVarGPUSkinCachePrintMemorySummary(
	TEXT("r.SkinCache.PrintMemorySummary"),
	GSkinCachePrintMemorySummary,
	TEXT("Print break down of memory usage.")
	TEXT(" 0: off (default),")
	TEXT(" 1: print when out of memory,")
	TEXT(" 2: print every frame"),
	ECVF_RenderThreadSafe
);

int32 GNumDispatchesToCapture = 0;
static FAutoConsoleVariableRef CVarGPUSkinCacheNumDispatchesToCapture(
	TEXT("r.SkinCache.Capture"),
	GNumDispatchesToCapture,
	TEXT("Trigger a render capture for the next skin cache dispatches."));

static int32 GGPUSkinCacheFlushCounter = 0;

const float MBSize = 1048576.f; // 1024 x 1024 bytes

static inline bool IsGPUSkinCacheEnable(EShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<int32> PerPlatformCVar(TEXT("r.SkinCache.Mode"));
	return (PerPlatformCVar.Get(Platform) != 0);
}

static inline bool IsGPUSkinCacheInclusive(EShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<int32> PerPlatformCVar(TEXT("r.SkinCache.DefaultBehavior"));
	return (PerPlatformCVar.Get(Platform) != 0);
}

bool ShouldWeCompileGPUSkinVFShaders(EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel)
{
	// If the skin cache is not available on this platform we need to compile GPU Skin VF shaders.
	if (IsGPUSkinCacheAvailable(Platform) == false)
	{
		return true;
	}

	// If the skin cache is not available on this platform we need to compile GPU Skin VF Shaders.
	if (IsGPUSkinCacheEnable(Platform) == false)
	{
		return true;
	}

	// If the skin cache has been globally disabled for all skeletal meshes we need to compile GPU Skin VF Shaders.
	if (IsGPUSkinCacheInclusive(Platform) == false)
	{
		return true;
	}

	// Some mobile GPUs (MALI) has a 64K elements limitation on texel buffers
	// This results in meshes with more than 64k vertices having their skin cache entries disabled at runtime.
	// We don't have a reliable way of checking this at cook time, so for mobile we must always cache skin cache
	// shaders so we have something to fall back to.
	if (FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		return true;
	}

	// If the skin cache is enabled and we've been asked to skip GPU Skin VF shaders.
	static FShaderPlatformCachedIniValue<bool> PerPlatformCVar(TEXT("r.SkinCache.SkipCompilingGPUSkinVF"));
	return (PerPlatformCVar.Get(Platform) == false);
}

ENGINE_API bool GPUSkinCacheNeedsDuplicatedVertices()
{
#if WITH_EDITOR // Duplicated vertices are used in the editor when merging meshes
	return true;
#else
	return GAllowDupedVertsForRecomputeTangents == 0;
#endif
}

// determine if during DispatchUpdateSkinning caching should occur
enum class EGPUSkinCacheDispatchFlags
{
	DispatchPrevPosition	= 1 << 0,
	DispatchPosition		= 1 << 1,
};

class FGPUSkinCacheEntry
{
public:
	FGPUSkinCacheEntry(FGPUSkinCache* InSkinCache, FSkeletalMeshObjectGPUSkin* InGPUSkin, FGPUSkinCache::FRWBuffersAllocation* InPositionAllocation, int32 InLOD, EGPUSkinCacheEntryMode InMode)
		: Mode(InMode)
		, PositionAllocation(InPositionAllocation)
		, SkinCache(InSkinCache)
		, GPUSkin(InGPUSkin)
		, MorphBuffer(0)
		, LOD(InLOD)
	{
		const TArray<FSkelMeshRenderSection>& Sections = InGPUSkin->GetRenderSections(LOD);
		DispatchData.AddDefaulted(Sections.Num());
		BatchElementsUserData.AddZeroed(Sections.Num());
		for (int32 Index = 0; Index < Sections.Num(); ++Index)
		{
			BatchElementsUserData[Index].SkinCacheEntry = this;
			BatchElementsUserData[Index].SectionIndex = Index;
		}

		UpdateSkinWeightBuffer();
	}

	~FGPUSkinCacheEntry()
	{
		check(!PositionAllocation);
	}

	struct FSectionDispatchData
	{
		FGPUSkinCache::FRWBufferTracker PositionTracker;

		FGPUBaseSkinVertexFactory* SourceVertexFactory = nullptr;
		FGPUSkinPassthroughVertexFactory* TargetVertexFactory = nullptr;

		// triangle index buffer (input for the RecomputeSkinTangents, might need special index buffer unique to position and normal, not considering UV/vertex color)
		FRHIShaderResourceView* IndexBuffer = nullptr;

		const FSkelMeshRenderSection* Section = nullptr;

		// for debugging / draw events, -1 if not set
		uint32 SectionIndex = -1;

		// 0:normal, 1:with morph target, 2:with APEX cloth (not yet implemented)
		uint16 SkinType = 0;

		// See EGPUSkinCacheDispatchFlags
		uint16 DispatchFlags = 0;

		uint32 UpdatedFrameNumber = 0;
		
		uint32 NumBoneInfluences = 0;

		// in floats (4 bytes)
		uint32 OutputStreamStart = 0;
		uint32 NumVertices = 0;

		// in vertices
		uint32 InputStreamStart = 0;
		uint32 NumTexCoords = 1;
		uint32 SelectedTexCoord = 0;

		FShaderResourceViewRHIRef TangentBufferSRV = nullptr;
		FShaderResourceViewRHIRef UVsBufferSRV = nullptr;
		FShaderResourceViewRHIRef ColorBufferSRV = nullptr;
		FShaderResourceViewRHIRef PositionBufferSRV = nullptr;
		FShaderResourceViewRHIRef ClothPositionsAndNormalsBuffer = nullptr;

		// skin weight input
		uint32 InputWeightStart = 0;

		// morph input
		uint32 MorphBufferOffset = 0;

        // cloth input
		uint32 ClothBufferOffset = 0;
        float ClothBlendWeight = 0.0f;
		uint32 ClothNumInfluencesPerVertex = 1;
        FMatrix44f ClothToLocal = FMatrix44f::Identity;
		FVector3f WorldScale = FVector3f::OneVector;

		// triangle index buffer (input for the RecomputeSkinTangents, might need special index buffer unique to position and normal, not considering UV/vertex color)
		uint32 IndexBufferOffsetValue = 0;
		uint32 NumTriangles = 0;
		uint32 RevisionNumber = 0;
		FGPUSkinCache::FSkinCacheRWBuffer* TangentBuffer = nullptr;
		FGPUSkinCache::FSkinCacheRWBuffer* IntermediateTangentBuffer = nullptr;
		FGPUSkinCache::FSkinCacheRWBuffer* IntermediateAccumulatedTangentBuffer = nullptr;
		uint32 IntermediateAccumulatedTangentBufferOffset = 0;
		FGPUSkinCache::FSkinCacheRWBuffer* PositionBuffer = nullptr;
		FGPUSkinCache::FSkinCacheRWBuffer* PreviousPositionBuffer = nullptr;

        // Handle duplicates
        FShaderResourceViewRHIRef DuplicatedIndicesIndices = nullptr;
        FShaderResourceViewRHIRef DuplicatedIndices = nullptr;

		FSectionDispatchData() = default;

		inline FGPUSkinCache::FSkinCacheRWBuffer* GetPreviousPositionRWBuffer() const
		{
			check(PreviousPositionBuffer);
			return PreviousPositionBuffer;
		}

		inline FGPUSkinCache::FSkinCacheRWBuffer* GetPositionRWBuffer() const
		{
			check(PositionBuffer);
			return PositionBuffer;
		}

		inline FGPUSkinCache::FSkinCacheRWBuffer* GetTangentRWBuffer() const
		{
			return TangentBuffer;
		}

		FGPUSkinCache::FSkinCacheRWBuffer* GetActiveTangentRWBuffer() const
		{
			// This is the buffer containing tangent results from the skinning CS pass
			return (IndexBuffer && IntermediateTangentBuffer) ? IntermediateTangentBuffer : TangentBuffer;
		}

		inline FGPUSkinCache::FSkinCacheRWBuffer* GetIntermediateAccumulatedTangentBuffer() const
		{
			check(IntermediateAccumulatedTangentBuffer);
			return IntermediateAccumulatedTangentBuffer;
		}

		void UpdateVertexFactoryDeclaration(FRHICommandListBase& RHICmdList)
		{
			FGPUSkinPassthroughVertexFactory::FAddVertexAttributeDesc Desc;
			Desc.FrameNumber = SourceVertexFactory->GetShaderData().UpdatedFrameNumber;
			Desc.VertexAttributes.Add(FGPUSkinPassthroughVertexFactory::VertexPosition);
			Desc.VertexAttributes.Add(FGPUSkinPassthroughVertexFactory::VertexTangent);
			Desc.SRVs[FGPUSkinPassthroughVertexFactory::Position] = GetPositionRWBuffer()->Buffer.SRV;
			Desc.SRVs[FGPUSkinPassthroughVertexFactory::PreviousPosition] = GetPreviousPositionRWBuffer()->Buffer.SRV;
			Desc.SRVs[FGPUSkinPassthroughVertexFactory::Tangent] = GetTangentRWBuffer()->Buffer.SRV;
			TargetVertexFactory->SetVertexAttributes(RHICmdList, SourceVertexFactory, Desc);
		}
	};

	void UpdateVertexFactoryDeclaration(FRHICommandListBase& RHICmdList, int32 Section)
	{
		DispatchData[Section].UpdateVertexFactoryDeclaration(RHICmdList);
	}

	inline FCachedGeometry::Section GetCachedGeometry(int32 SectionIndex) const
	{
		FCachedGeometry::Section MeshSection;
		if (SectionIndex >= 0 && SectionIndex < DispatchData.Num())
		{
			const FSkelMeshRenderSection& Section = *DispatchData[SectionIndex].Section;
			MeshSection.PositionBuffer = DispatchData[SectionIndex].PositionBuffer->Buffer.SRV;
			MeshSection.PreviousPositionBuffer = DispatchData[SectionIndex].PreviousPositionBuffer->Buffer.SRV;
			MeshSection.UVsBuffer = DispatchData[SectionIndex].UVsBufferSRV;
			MeshSection.TotalVertexCount = DispatchData[SectionIndex].PositionBuffer->Buffer.NumBytes / (sizeof(float) * 3);
			MeshSection.NumPrimitives = Section.NumTriangles;
			MeshSection.NumVertices = Section.NumVertices;
			MeshSection.IndexBaseIndex = Section.BaseIndex;
			MeshSection.VertexBaseIndex = Section.BaseVertexIndex;
			MeshSection.IndexBuffer = nullptr;
			MeshSection.TotalIndexCount = 0;
			MeshSection.LODIndex = 0;
			MeshSection.SectionIndex = SectionIndex;
		}
		return MeshSection;
	}

	bool IsSectionValid(int32 Section) const
	{
		const FSectionDispatchData& SectionData = DispatchData[Section];
		return SectionData.SectionIndex == Section;
	}

	bool IsSourceFactoryValid(int32 Section, FGPUBaseSkinVertexFactory* SourceVertexFactory) const
	{
		const FSectionDispatchData& SectionData = DispatchData[Section];
		return SectionData.SourceVertexFactory == SourceVertexFactory;
	}

	bool IsValid(FSkeletalMeshObjectGPUSkin* InSkin, int32 InLOD) const
	{
		return GPUSkin == InSkin && LOD == InLOD;
	}

	void UpdateSkinWeightBuffer()
	{
		FSkinWeightVertexBuffer* WeightBuffer = GPUSkin->GetSkinWeightVertexBuffer(LOD);
		bUse16BitBoneIndex = WeightBuffer->Use16BitBoneIndex();
		bUse16BitBoneWeight = WeightBuffer->Use16BitBoneWeight();
		InputWeightIndexSize = WeightBuffer->GetBoneIndexByteSize() | (WeightBuffer->GetBoneWeightByteSize() << 8);
		InputWeightStride = WeightBuffer->GetConstantInfluencesVertexStride();
		InputWeightStreamSRV = WeightBuffer->GetDataVertexBuffer()->GetSRV();
		InputWeightLookupStreamSRV = WeightBuffer->GetLookupVertexBuffer()->GetSRV();
				
		if (WeightBuffer->GetBoneInfluenceType() == GPUSkinBoneInfluenceType::DefaultBoneInfluence)
		{
			int32 MaxBoneInfluences = WeightBuffer->GetMaxBoneInfluences();
			BoneInfluenceType = MaxBoneInfluences > MAX_INFLUENCES_PER_STREAM ? 1 : 0;
		}
		else
		{
			BoneInfluenceType = 2;
		}
	}

	void SetupSection(
		int32 SectionIndex,
		FGPUSkinCache::FRWBuffersAllocation* InPositionAllocation,
		FSkelMeshRenderSection* Section,
		const FMorphVertexBuffer* MorphVertexBuffer,
		const FSkeletalMeshVertexClothBuffer* ClothVertexBuffer,
		uint32 NumVertices,
		uint32 InputStreamStart,
		FGPUBaseSkinVertexFactory* InSourceVertexFactory,
		FGPUSkinPassthroughVertexFactory* InTargetVertexFactory,
		uint32 InIntermediateAccumulatedTangentBufferOffset,
		const FClothSimulData* SimData)
	{
		//UE_LOG(LogSkinCache, Warning, TEXT("*** SetupSection E %p Alloc %p Sec %d(%p) LOD %d"), this, InAllocation, SectionIndex, Section, LOD);
		FSectionDispatchData& Data = DispatchData[SectionIndex];
		check(!Data.PositionTracker.Allocation || Data.PositionTracker.Allocation == InPositionAllocation);

		Data.PositionTracker.Allocation = InPositionAllocation;

		Data.SectionIndex = SectionIndex;
		Data.Section = Section;

		FSkeletalMeshRenderData& SkelMeshRenderData = GPUSkin->GetSkeletalMeshRenderData();
		FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData.LODRenderData[LOD];
		check(Data.SectionIndex == LodData.FindSectionIndex(*Section));

		Data.NumVertices = NumVertices;
		const bool bMorph = MorphVertexBuffer && MorphVertexBuffer->SectionIds.Contains(SectionIndex);
		if (bMorph)
		{
			// in bytes
			const uint32 MorphStride = sizeof(FMorphGPUSkinVertex);

			// see GPU code "check(MorphStride == sizeof(float) * 6);"
			check(MorphStride == sizeof(float) * 6);

			Data.MorphBufferOffset = Section->BaseVertexIndex;
		}

		if (ClothVertexBuffer)
		{
			constexpr int32 ClothLODBias0 = 0;  // Use the same cloth LOD mapping (= 0 bias) to get the number of deformer weights
			const uint32 NumWrapDeformerWeights = Data.Section->ClothMappingDataLODs.Num() ? Data.Section->ClothMappingDataLODs[ClothLODBias0].Num() : 0;
			// NumInfluencesPerVertex should be a whole integer
			check(NumWrapDeformerWeights % Data.NumVertices == 0);
			Data.ClothNumInfluencesPerVertex = NumWrapDeformerWeights / Data.NumVertices;

			if (ClothVertexBuffer->GetClothIndexMapping().Num() > SectionIndex)
			{

				const FClothBufferIndexMapping& ClothBufferIndexMapping = ClothVertexBuffer->GetClothIndexMapping()[SectionIndex];

				check(SimData->LODIndex != INDEX_NONE && SimData->LODIndex <= LOD);
				const uint32 ClothLODBias = (uint32)(LOD - SimData->LODIndex);

				const uint32 ClothBufferOffset = ClothBufferIndexMapping.MappingOffset + ClothBufferIndexMapping.LODBiasStride * ClothLODBias;

				// Set the buffer offset depending on whether enough deformer mapping data exists (RaytracingMinLOD/RaytracingLODBias/ClothLODBiasMode settings)
				const uint32 NumInfluences = NumVertices ? ClothBufferIndexMapping.LODBiasStride / NumVertices : 1;
				Data.ClothBufferOffset = (ClothBufferOffset + NumVertices * NumInfluences <= ClothVertexBuffer->GetNumVertices()) ?
					ClothBufferOffset :                     // If the offset is valid, set the calculated LODBias offset
					ClothBufferIndexMapping.MappingOffset;  // Otherwise fallback to a 0 ClothLODBias to prevent from reading pass the buffer (but still raytrace broken shadows/reflections/etc.)
			}
		}

		//INC_DWORD_STAT(STAT_GPUSkinCache_TotalNumChunks);

		// SkinType 0:normal, 1:with morph target, 2:with cloth
		Data.SkinType = ClothVertexBuffer ? 2 : (bMorph ? 1 : 0);
		Data.InputStreamStart = InputStreamStart;
		Data.OutputStreamStart = Section->BaseVertexIndex;

		Data.TangentBufferSRV = InSourceVertexFactory->GetTangentsSRV();
		Data.UVsBufferSRV = InSourceVertexFactory->GetTextureCoordinatesSRV();
		Data.ColorBufferSRV = InSourceVertexFactory->GetColorComponentsSRV();
		Data.NumTexCoords = InSourceVertexFactory->GetNumTexCoords();
		Data.PositionBufferSRV = InSourceVertexFactory->GetPositionsSRV();

		Data.NumBoneInfluences = InSourceVertexFactory->GetNumBoneInfluences();
		check(Data.TangentBufferSRV && Data.PositionBufferSRV);

		// weight buffer
		Data.InputWeightStart = (InputWeightStride * Section->BaseVertexIndex) / sizeof(float);
		Data.SourceVertexFactory = InSourceVertexFactory;
		Data.TargetVertexFactory = InTargetVertexFactory;

		InTargetVertexFactory->ResetVertexAttributes();

		int32 RecomputeTangentsMode = GSkinCacheRecomputeTangents;
		if (RecomputeTangentsMode > 0)
		{
			if (Section->bRecomputeTangent || RecomputeTangentsMode == 1)
			{
				FRawStaticIndexBuffer16or32Interface* IndexBuffer = LodData.MultiSizeIndexContainer.GetIndexBuffer();
				Data.IndexBuffer = IndexBuffer->GetSRV();
				if (Data.IndexBuffer)
				{
					Data.NumTriangles = Section->NumTriangles;
					Data.IndexBufferOffsetValue = Section->BaseIndex;
					Data.IntermediateAccumulatedTangentBufferOffset = InIntermediateAccumulatedTangentBufferOffset;
				}
			}
		}
	}

#if RHI_RAYTRACING
	void GetRayTracingSegmentVertexBuffers(TArray<FBufferRHIRef>& OutVertexBuffers) const
	{
		OutVertexBuffers.SetNum(DispatchData.Num());
		for (int32 SectionIdx = 0; SectionIdx < DispatchData.Num(); SectionIdx++)
		{
			OutVertexBuffers[SectionIdx] = DispatchData[SectionIdx].PositionBuffer->Buffer.Buffer;
		}
	}
#endif // RHI_RAYTRACING

	TArray<FSectionDispatchData>& GetDispatchData() { return DispatchData; }
	TArray<FSectionDispatchData> const& GetDispatchData() const { return DispatchData; }

protected:
	EGPUSkinCacheEntryMode Mode;
	FGPUSkinCache::FRWBuffersAllocation* PositionAllocation;
	FGPUSkinCache* SkinCache;
	TArray<FSkinBatchVertexFactoryUserData> BatchElementsUserData;
	TArray<FSectionDispatchData> DispatchData;
	FSkeletalMeshObjectGPUSkin* GPUSkin;
	int BoneInfluenceType;
	bool bUse16BitBoneIndex;
	bool bUse16BitBoneWeight;
	bool bQueuedForDispatch = false;
	uint32 InputWeightIndexSize;
	uint32 InputWeightStride;
	FShaderResourceViewRHIRef InputWeightStreamSRV;
	FShaderResourceViewRHIRef InputWeightLookupStreamSRV;
	FRHIShaderResourceView* MorphBuffer;
	FShaderResourceViewRHIRef ClothBuffer;
	int32 LOD;

	friend class FGPUSkinCache;
	friend class FBaseGPUSkinCacheCS;
	friend class FBaseRecomputeTangentsPerTriangleShader;
};

class FBaseGPUSkinCacheCS : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FBaseGPUSkinCacheCS, NonVirtual);
public:
	FBaseGPUSkinCacheCS() {}

	FBaseGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		//DebugParameter.Bind(Initializer.ParameterMap, TEXT("DebugParameter"));

		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		SkinCacheStart.Bind(Initializer.ParameterMap, TEXT("SkinCacheStart"));
		BoneMatrices.Bind(Initializer.ParameterMap, TEXT("BoneMatrices"));
		TangentInputBuffer.Bind(Initializer.ParameterMap, TEXT("TangentInputBuffer"));
		PositionInputBuffer.Bind(Initializer.ParameterMap, TEXT("PositionInputBuffer"));

		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));

		NumBoneInfluences.Bind(Initializer.ParameterMap, TEXT("NumBoneInfluences"));
		InputWeightIndexSize.Bind(Initializer.ParameterMap, TEXT("InputWeightIndexSize"));
		InputWeightStart.Bind(Initializer.ParameterMap, TEXT("InputWeightStart"));
		InputWeightStride.Bind(Initializer.ParameterMap, TEXT("InputWeightStride"));
		InputWeightStream.Bind(Initializer.ParameterMap, TEXT("InputWeightStream"));
		InputWeightLookupStream.Bind(Initializer.ParameterMap, TEXT("InputWeightLookupStream"));

		PositionBufferUAV.Bind(Initializer.ParameterMap, TEXT("PositionBufferUAV"));
		TangentBufferUAV.Bind(Initializer.ParameterMap, TEXT("TangentBufferUAV"));

		MorphBuffer.Bind(Initializer.ParameterMap, TEXT("MorphBuffer"));
		MorphBufferOffset.Bind(Initializer.ParameterMap, TEXT("MorphBufferOffset"));
		SkinCacheDebug.Bind(Initializer.ParameterMap, TEXT("SkinCacheDebug"));

		ClothBuffer.Bind(Initializer.ParameterMap, TEXT("ClothBuffer"));
		ClothPositionsAndNormalsBuffer.Bind(Initializer.ParameterMap, TEXT("ClothPositionsAndNormalsBuffer"));
		ClothBufferOffset.Bind(Initializer.ParameterMap, TEXT("ClothBufferOffset"));
		ClothBlendWeight.Bind(Initializer.ParameterMap, TEXT("ClothBlendWeight"));
		ClothToLocal.Bind(Initializer.ParameterMap, TEXT("ClothToLocal"));
		ClothNumInfluencesPerVertex.Bind(Initializer.ParameterMap, TEXT("ClothNumInfluencesPerVertex"));
		WorldScale.Bind(Initializer.ParameterMap, TEXT("WorldScale"));
	}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		const FVertexBufferAndSRV& BoneBuffer,
		FGPUSkinCacheEntry* Entry,
		const FGPUSkinCacheEntry::FSectionDispatchData& DispatchData,
		FRHIUnorderedAccessView* PositionUAV,
		FRHIUnorderedAccessView* TangentUAV
		)
	{
		SetShaderValue(BatchedParameters, NumVertices, DispatchData.NumVertices);
		SetShaderValue(BatchedParameters, InputStreamStart, DispatchData.InputStreamStart);

		check(BoneBuffer.VertexBufferSRV);
		SetSRVParameter(BatchedParameters, BoneMatrices, BoneBuffer.VertexBufferSRV);

		SetSRVParameter(BatchedParameters, TangentInputBuffer, DispatchData.TangentBufferSRV);
		SetSRVParameter(BatchedParameters, PositionInputBuffer, DispatchData.PositionBufferSRV);

		SetShaderValue(BatchedParameters, NumBoneInfluences, DispatchData.NumBoneInfluences);
		SetShaderValue(BatchedParameters, InputWeightIndexSize, Entry->InputWeightIndexSize);
		SetShaderValue(BatchedParameters, InputWeightStart, DispatchData.InputWeightStart);
		SetShaderValue(BatchedParameters, InputWeightStride, Entry->InputWeightStride);
		SetSRVParameter(BatchedParameters, InputWeightStream, Entry->InputWeightStreamSRV);
		SetSRVParameter(BatchedParameters, InputWeightLookupStream, Entry->InputWeightLookupStreamSRV);

		// output UAV
		SetUAVParameter(BatchedParameters, PositionBufferUAV, PositionUAV);
		SetUAVParameter(BatchedParameters, TangentBufferUAV, TangentUAV);
		SetShaderValue(BatchedParameters, SkinCacheStart, DispatchData.OutputStreamStart);

		const bool bMorph = DispatchData.SkinType == 1;
		if (bMorph)
		{
			SetSRVParameter(BatchedParameters, MorphBuffer, Entry->MorphBuffer);
			SetShaderValue(BatchedParameters, MorphBufferOffset, DispatchData.MorphBufferOffset);
		}

		const bool bCloth = DispatchData.SkinType == 2;
		if (bCloth)
		{
			SetSRVParameter(BatchedParameters, ClothBuffer, Entry->ClothBuffer);
			SetSRVParameter(BatchedParameters, ClothPositionsAndNormalsBuffer, DispatchData.ClothPositionsAndNormalsBuffer);
			SetShaderValue(BatchedParameters, ClothBufferOffset, DispatchData.ClothBufferOffset);
			SetShaderValue(BatchedParameters, ClothBlendWeight, DispatchData.ClothBlendWeight);
			SetShaderValue(BatchedParameters, ClothToLocal, DispatchData.ClothToLocal);
			SetShaderValue(BatchedParameters, ClothNumInfluencesPerVertex, DispatchData.ClothNumInfluencesPerVertex);
			SetShaderValue(BatchedParameters, WorldScale, DispatchData.WorldScale);
		}

		SetShaderValue(BatchedParameters, SkinCacheDebug, CVarGPUSkinCacheDebug.GetValueOnRenderThread());
	}

	void UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds)
	{
		UnsetUAVParameter(BatchedUnbinds, PositionBufferUAV);
		UnsetUAVParameter(BatchedUnbinds, TangentBufferUAV);
	}

private:
	
	LAYOUT_FIELD(FShaderParameter, NumVertices)
	LAYOUT_FIELD(FShaderParameter, SkinCacheDebug)
	LAYOUT_FIELD(FShaderParameter, InputStreamStart)
	LAYOUT_FIELD(FShaderParameter, SkinCacheStart)

	//LAYOUT_FIELD(FShaderParameter, DebugParameter)

	LAYOUT_FIELD(FShaderUniformBufferParameter, SkinUniformBuffer)

	LAYOUT_FIELD(FShaderResourceParameter, BoneMatrices)
	LAYOUT_FIELD(FShaderResourceParameter, TangentInputBuffer)
	LAYOUT_FIELD(FShaderResourceParameter, PositionInputBuffer)
	LAYOUT_FIELD(FShaderResourceParameter, PositionBufferUAV)
	LAYOUT_FIELD(FShaderResourceParameter, TangentBufferUAV)

	LAYOUT_FIELD(FShaderParameter, NumBoneInfluences);
	LAYOUT_FIELD(FShaderParameter, InputWeightIndexSize);
	LAYOUT_FIELD(FShaderParameter, InputWeightStart)
	LAYOUT_FIELD(FShaderParameter, InputWeightStride)
	LAYOUT_FIELD(FShaderResourceParameter, InputWeightStream)
	LAYOUT_FIELD(FShaderResourceParameter, InputWeightLookupStream);

	LAYOUT_FIELD(FShaderResourceParameter, MorphBuffer)
	LAYOUT_FIELD(FShaderParameter, MorphBufferOffset)

	LAYOUT_FIELD(FShaderResourceParameter, ClothBuffer)
	LAYOUT_FIELD(FShaderResourceParameter, ClothPositionsAndNormalsBuffer)
	LAYOUT_FIELD(FShaderParameter, ClothBufferOffset)
	LAYOUT_FIELD(FShaderParameter, ClothBlendWeight)
	LAYOUT_FIELD(FShaderParameter, ClothToLocal)
	LAYOUT_FIELD(FShaderParameter, ClothNumInfluencesPerVertex)
	LAYOUT_FIELD(FShaderParameter, WorldScale)
};

/** Compute shader that skins a batch of vertices. */
// @param SkinType 0:normal, 1:with morph targets calculated outside the cache, 2: with cloth, 3:with morph target calculated inside the cache (not yet implemented)
//        BoneInfluenceType 0:normal, 1:extra bone influences, 2:unlimited bone influences
//        BoneIndex16 0: 8-bit indices, 1: 16-bit indices
//        BoneWeights16 0: 8-bit weights, 1: 16-bit weights
template <int Permutation>
class TGPUSkinCacheCS : public FBaseGPUSkinCacheCS
{
	constexpr static bool bBoneWeights16 = (32 == (Permutation & 32));
	constexpr static bool bBoneIndex16 = (16 == (Permutation & 16));
	constexpr static bool bUnlimitedBoneInfluence = (8 == (Permutation & 12));
	constexpr static bool bUseExtraBoneInfluencesT = (4 == (Permutation & 12));
	constexpr static bool bApexCloth = (2 == (Permutation & 3));
    constexpr static bool bMorphBlend = (1 == (Permutation & 3));

	DECLARE_SHADER_TYPE(TGPUSkinCacheCS, Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsGPUSkinCacheAvailable(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_UNLIMITED_BONE_INFLUENCE"), bUnlimitedBoneInfluence);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), bUseExtraBoneInfluencesT);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_MORPH_BLEND"), bMorphBlend);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_APEX_CLOTH"), bApexCloth);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_X"), FGPUSkinCache::RWTangentXOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_Z"), FGPUSkinCache::RWTangentZOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_BONE_INDEX_UINT16"), bBoneIndex16);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_BONE_WEIGHTS_UINT16"), bBoneWeights16);
	}

	TGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseGPUSkinCacheCS(Initializer)
	{
	}

	TGPUSkinCacheCS()
	{
	}
};

#define SKIN_CACHE_SHADER_IDX(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_, _SKIN_TYPE_) (_WEIGHT16_ * 32 + _INDEX16_ * 16 + _INFLUENCE_TYPE_ * 4 + _SKIN_TYPE_)

#define SKIN_CACHE_SHADER_ALL_SKIN_TYPES(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_) \
	SKIN_CACHE_SHADER(SKIN_CACHE_SHADER_IDX(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_, 0)); \
	SKIN_CACHE_SHADER(SKIN_CACHE_SHADER_IDX(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_, 1)); \
	SKIN_CACHE_SHADER(SKIN_CACHE_SHADER_IDX(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_, 2))

// NOTE: Bone influence type 2 (multiple) does not require a 16-bit index or weight permutation.
#define SKIN_CACHE_SHADER_ALL() \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 0, 0); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 0, 1); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 1, 0); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 1, 1); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(1, 0, 0); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(1, 0, 1); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(1, 1, 0); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(1, 1, 1); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 0, 2);

#define SKIN_CACHE_SHADER(_SHADER_IDX_) \
	IMPLEMENT_SHADER_TYPE(template<>, TGPUSkinCacheCS<_SHADER_IDX_>, TEXT("/Engine/Private/GpuSkinCacheComputeShader.usf"), TEXT("SkinCacheUpdateBatchCS"), SF_Compute) 
SKIN_CACHE_SHADER_ALL()
#undef SKIN_CACHE_SHADER

FGPUSkinCache::FGPUSkinCache(ERHIFeatureLevel::Type InFeatureLevel, bool bInRequiresMemoryLimit, UWorld* InWorld)
	: UsedMemoryInBytes(0)
	, ExtraRequiredMemory(0)
	, FlushCounter(0)
	, bRequiresMemoryLimit(bInRequiresMemoryLimit)
	, CurrentStagingBufferIndex(0)
	, FeatureLevel(InFeatureLevel)
	, World(InWorld)
{
	check(World);
}

FGPUSkinCache::~FGPUSkinCache()
{
	Cleanup();
}

void FGPUSkinCache::Cleanup()
{
	for (int32 Index = 0; Index < StagingBuffers.Num(); ++Index)
	{
		StagingBuffers[Index].Release();
	}

	while (Entries.Num() > 0)
	{
		Release(Entries.Last());
	}
	ensure(Allocations.Num() == 0);
}

void FGPUSkinCache::TransitionAllToReadable(FRHICommandList& RHICmdList, const TArray<FSkinCacheRWBuffer*>& BuffersToTransitionToRead)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FGPUSkinCache::TransitionAllToReadable);

	if (BuffersToTransitionToRead.Num() > 0)
	{
		TArray<FRHITransitionInfo, SceneRenderingAllocator> UAVs;
		UAVs.Reserve(BuffersToTransitionToRead.Num());
		for (TArray<FSkinCacheRWBuffer*>::TConstIterator SetIt(BuffersToTransitionToRead); SetIt; ++SetIt)
		{
			FSkinCacheRWBuffer* Buffer = *SetIt;
			constexpr ERHIAccess ToState = ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask;
			if (Buffer->AccessState != ToState)
			{
				UAVs.Add(Buffer->UpdateAccessState(ToState));
			}
		}
		RHICmdList.Transition(UAVs);
	}
}

/** base of the FRecomputeTangentsPerTrianglePassCS class */
class FBaseRecomputeTangentsPerTriangleShader : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FBaseRecomputeTangentsPerTriangleShader, NonVirtual);
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return IsGPUSkinCacheAvailable(Parameters.Platform);
	}

	static const uint32 ThreadGroupSizeX = 64;

	FBaseRecomputeTangentsPerTriangleShader()
	{}

	FBaseRecomputeTangentsPerTriangleShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		IntermediateAccumBufferOffset.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferOffset"));
		NumTriangles.Bind(Initializer.ParameterMap, TEXT("NumTriangles"));
		GPUPositionCacheBuffer.Bind(Initializer.ParameterMap, TEXT("GPUPositionCacheBuffer"));
		GPUTangentCacheBuffer.Bind(Initializer.ParameterMap, TEXT("GPUTangentCacheBuffer"));
		SkinCacheStart.Bind(Initializer.ParameterMap, TEXT("SkinCacheStart"));
		IndexBuffer.Bind(Initializer.ParameterMap, TEXT("IndexBuffer"));
		IndexBufferOffset.Bind(Initializer.ParameterMap, TEXT("IndexBufferOffset"));

		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		NumTexCoords.Bind(Initializer.ParameterMap, TEXT("NumTexCoords"));
		SelectedTexCoord.Bind(Initializer.ParameterMap, TEXT("SelectedTexCoord"));
		TangentInputBuffer.Bind(Initializer.ParameterMap, TEXT("TangentInputBuffer"));
		UVsInputBuffer.Bind(Initializer.ParameterMap, TEXT("UVsInputBuffer"));

        DuplicatedIndices.Bind(Initializer.ParameterMap, TEXT("DuplicatedIndices"));
        DuplicatedIndicesIndices.Bind(Initializer.ParameterMap, TEXT("DuplicatedIndicesIndices"));
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, FGPUSkinCacheEntry* Entry, const FGPUSkinCacheEntry::FSectionDispatchData& DispatchData, const FRWBuffer& StagingBuffer)
	{
//later		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View);

		SetShaderValue(BatchedParameters, NumTriangles, DispatchData.NumTriangles);

		SetSRVParameter(BatchedParameters, GPUPositionCacheBuffer, DispatchData.GetPositionRWBuffer()->Buffer.SRV);
		SetSRVParameter(BatchedParameters, GPUTangentCacheBuffer, DispatchData.GetActiveTangentRWBuffer()->Buffer.SRV);
		SetSRVParameter(BatchedParameters, UVsInputBuffer, DispatchData.UVsBufferSRV);

		SetShaderValue(BatchedParameters, SkinCacheStart, DispatchData.OutputStreamStart);

		SetSRVParameter(BatchedParameters, IndexBuffer, DispatchData.IndexBuffer);
		SetShaderValue(BatchedParameters, IndexBufferOffset, DispatchData.IndexBufferOffsetValue);
		
		SetShaderValue(BatchedParameters, InputStreamStart, DispatchData.InputStreamStart);
		SetShaderValue(BatchedParameters, NumTexCoords, DispatchData.NumTexCoords);
		SetShaderValue(BatchedParameters, SelectedTexCoord, DispatchData.SelectedTexCoord);
		SetSRVParameter(BatchedParameters, TangentInputBuffer, DispatchData.TangentBufferSRV);
		SetSRVParameter(BatchedParameters, TangentInputBuffer, DispatchData.UVsBufferSRV);

		// UAV
		SetUAVParameter(BatchedParameters, IntermediateAccumBufferUAV, StagingBuffer.UAV);
		SetShaderValue(BatchedParameters, IntermediateAccumBufferOffset, GRecomputeTangentsParallelDispatch * DispatchData.IntermediateAccumulatedTangentBufferOffset);

        if (!GAllowDupedVertsForRecomputeTangents)
        {
		    SetSRVParameter(BatchedParameters, DuplicatedIndices, DispatchData.DuplicatedIndices);
            SetSRVParameter(BatchedParameters, DuplicatedIndicesIndices, DispatchData.DuplicatedIndicesIndices);
        }
	}

	void UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds)
	{
		UnsetUAVParameter(BatchedUnbinds, IntermediateAccumBufferUAV);
	}

	LAYOUT_FIELD(FShaderResourceParameter, IntermediateAccumBufferUAV);
	LAYOUT_FIELD(FShaderParameter, IntermediateAccumBufferOffset);
	LAYOUT_FIELD(FShaderParameter, NumTriangles);
	LAYOUT_FIELD(FShaderResourceParameter, GPUPositionCacheBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, GPUTangentCacheBuffer);
	LAYOUT_FIELD(FShaderParameter, SkinCacheStart);
	LAYOUT_FIELD(FShaderResourceParameter, IndexBuffer);
	LAYOUT_FIELD(FShaderParameter, IndexBufferOffset);
	LAYOUT_FIELD(FShaderParameter, InputStreamStart);
	LAYOUT_FIELD(FShaderParameter, NumTexCoords);
	LAYOUT_FIELD(FShaderParameter, SelectedTexCoord);
	LAYOUT_FIELD(FShaderResourceParameter, TangentInputBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, UVsInputBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, DuplicatedIndices);
	LAYOUT_FIELD(FShaderResourceParameter, DuplicatedIndicesIndices);
};

/** Encapsulates the RecomputeSkinTangents compute shader. */
template <int Permutation>
class FRecomputeTangentsPerTrianglePassCS : public FBaseRecomputeTangentsPerTriangleShader
{
    constexpr static bool bMergeDuplicatedVerts = (2 == (Permutation & 2));
	constexpr static bool bFullPrecisionUV = (1 == (Permutation & 1));

	DECLARE_SHADER_TYPE(FRecomputeTangentsPerTrianglePassCS, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MERGE_DUPLICATED_VERTICES"), bMergeDuplicatedVerts);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
		OutEnvironment.SetDefine(TEXT("FULL_PRECISION_UV"), bFullPrecisionUV);
	}

	FRecomputeTangentsPerTrianglePassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseRecomputeTangentsPerTriangleShader(Initializer)
	{
	}

	FRecomputeTangentsPerTrianglePassCS()
	{}
};

IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerTrianglePassCS<0>, TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerTrianglePassCS<1>, TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerTrianglePassCS<2>, TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerTrianglePassCS<3>, TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf"), TEXT("MainCS"), SF_Compute);

/** Encapsulates the RecomputeSkinTangentsResolve compute shader. */
class FBaseRecomputeTangentsPerVertexShader : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FBaseRecomputeTangentsPerVertexShader, NonVirtual);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return IsGPUSkinCacheAvailable(Parameters.Platform);
	}

	static const uint32 ThreadGroupSizeX = 64;

	LAYOUT_FIELD(FShaderResourceParameter, IntermediateAccumBufferUAV);
	LAYOUT_FIELD(FShaderParameter, IntermediateAccumBufferOffset);
	LAYOUT_FIELD(FShaderResourceParameter, TangentBufferUAV);
	LAYOUT_FIELD(FShaderResourceParameter, TangentInputBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, ColorInputBuffer);
	LAYOUT_FIELD(FShaderParameter, SkinCacheStart);
	LAYOUT_FIELD(FShaderParameter, NumVertices);
	LAYOUT_FIELD(FShaderParameter, InputStreamStart);
	LAYOUT_FIELD(FShaderParameter, VertexColorChannel); // which channel to use to read mask colors (0-R, 1-G, 2-B)

	FBaseRecomputeTangentsPerVertexShader() {}

	FBaseRecomputeTangentsPerVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		IntermediateAccumBufferOffset.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferOffset"));
		TangentBufferUAV.Bind(Initializer.ParameterMap, TEXT("TangentBufferUAV"));
		TangentInputBuffer.Bind(Initializer.ParameterMap, TEXT("TangentInputBuffer"));
		ColorInputBuffer.Bind(Initializer.ParameterMap, TEXT("ColorInputBuffer"));
		SkinCacheStart.Bind(Initializer.ParameterMap, TEXT("SkinCacheStart"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		VertexColorChannel.Bind(Initializer.ParameterMap, TEXT("VertexColorChannel"));
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, FGPUSkinCacheEntry* Entry, const FGPUSkinCacheEntry::FSectionDispatchData& DispatchData, const FRWBuffer& StagingBuffer)
	{
		check(StagingBuffer.UAV);

		//later		FGlobalShader::SetParameters<FViewUniformShaderParameters>(BatchedParameters, View);

		SetShaderValue(BatchedParameters, SkinCacheStart, DispatchData.OutputStreamStart);
		SetShaderValue(BatchedParameters, NumVertices, DispatchData.NumVertices);
		SetShaderValue(BatchedParameters, InputStreamStart, DispatchData.InputStreamStart);
		SetShaderValue(BatchedParameters, VertexColorChannel, uint32(DispatchData.Section->RecomputeTangentsVertexMaskChannel));

		// UAVs
		SetUAVParameter(BatchedParameters, IntermediateAccumBufferUAV, StagingBuffer.UAV);
		SetShaderValue(BatchedParameters, IntermediateAccumBufferOffset, GRecomputeTangentsParallelDispatch * DispatchData.IntermediateAccumulatedTangentBufferOffset);
		SetUAVParameter(BatchedParameters, TangentBufferUAV, DispatchData.GetTangentRWBuffer()->Buffer.UAV);

		SetSRVParameter(BatchedParameters, TangentInputBuffer, DispatchData.IntermediateTangentBuffer ? DispatchData.IntermediateTangentBuffer->Buffer.SRV : nullptr);

		SetSRVParameter(BatchedParameters, ColorInputBuffer, DispatchData.ColorBufferSRV);
	}

	void UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds)
	{
		UnsetUAVParameter(BatchedUnbinds, TangentBufferUAV);
		UnsetUAVParameter(BatchedUnbinds, IntermediateAccumBufferUAV);
	}
};

template <int Permutation>
class FRecomputeTangentsPerVertexPassCS : public FBaseRecomputeTangentsPerVertexShader
{
	DECLARE_SHADER_TYPE(FRecomputeTangentsPerVertexPassCS, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		// this pass cannot read the input as it doesn't have the permutation
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_X"), FGPUSkinCache::RWTangentXOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_Z"), FGPUSkinCache::RWTangentZOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
		OutEnvironment.SetDefine(TEXT("BLEND_USING_VERTEX_COLOR"), Permutation);
	}

	FRecomputeTangentsPerVertexPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseRecomputeTangentsPerVertexShader(Initializer)
	{
	}

	FRecomputeTangentsPerVertexPassCS()
	{}
};

IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerVertexPassCS<0>, TEXT("/Engine/Private/RecomputeTangentsPerVertexPass.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerVertexPassCS<1>, TEXT("/Engine/Private/RecomputeTangentsPerVertexPass.usf"), TEXT("MainCS"), SF_Compute);

void FGPUSkinCache::DispatchUpdateSkinTangents(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* Entry, int32 SectionIndex, FSkinCacheRWBuffer*& StagingBuffer, bool bTrianglePass)
{
	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[SectionIndex];

	FSkeletalMeshRenderData& SkelMeshRenderData = Entry->GPUSkin->GetSkeletalMeshRenderData();
	const int32 LODIndex = Entry->LOD;
	FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData.LODRenderData[LODIndex];
	const FString RayTracingTag = (Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""));

	if (bTrianglePass)
	{
		if (!GRecomputeTangentsParallelDispatch)
		{	
			if (StagingBuffers.Num() != GNumTangentIntermediateBuffers)
			{
				// Release extra buffers if shrinking
				for (int32 Index = GNumTangentIntermediateBuffers; Index < StagingBuffers.Num(); ++Index)
				{
					StagingBuffers[Index].Release();
				}
				StagingBuffers.SetNum(GNumTangentIntermediateBuffers, EAllowShrinking::No);
			}

			// no need to clear the staging buffer because we create it cleared and clear it after each usage in the per vertex pass
			uint32 NumIntsPerBuffer = DispatchData.NumVertices * FGPUSkinCache::IntermediateAccumBufferNumInts;
			CurrentStagingBufferIndex = (CurrentStagingBufferIndex + 1) % StagingBuffers.Num();
			StagingBuffer = &StagingBuffers[CurrentStagingBufferIndex];
			if (StagingBuffer->Buffer.NumBytes < NumIntsPerBuffer * sizeof(uint32))
			{
				StagingBuffer->Release();
				StagingBuffer->Buffer.Initialize(RHICmdList, TEXT("SkinTangentIntermediate"), sizeof(int32), NumIntsPerBuffer, PF_R32_SINT, BUF_UnorderedAccess);
				RHICmdList.BindDebugLabelName(StagingBuffer->Buffer.UAV, TEXT("SkinTangentIntermediate"));

				const uint32 MemSize = NumIntsPerBuffer * sizeof(uint32);
				SET_MEMORY_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed, MemSize);

				// The UAV must be zero-filled. We leave it zeroed after each round (see RecomputeTangentsPerVertexPass.usf), so this is only needed on when the buffer is first created.
				RHICmdList.ClearUAVUint(StagingBuffer->Buffer.UAV, FUintVector4(0, 0, 0, 0));
			}
		}

		{
			auto* GlobalShaderMap = GetGlobalShaderMap(GetFeatureLevel());
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<0>> ComputeShader00(GlobalShaderMap);
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<1>> ComputeShader01(GlobalShaderMap);
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<2>> ComputeShader10(GlobalShaderMap);
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<3>> ComputeShader11(GlobalShaderMap);

			bool bFullPrecisionUV = LodData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs();

			TShaderRef<FBaseRecomputeTangentsPerTriangleShader> Shader;

			if (bFullPrecisionUV)
			{
				if (GAllowDupedVertsForRecomputeTangents) Shader = ComputeShader01;
				else Shader = ComputeShader11;
			}
			else
			{
				if (GAllowDupedVertsForRecomputeTangents) Shader = ComputeShader00;
				else Shader = ComputeShader10;
			}

			check(Shader.IsValid());

			uint32 NumTriangles = DispatchData.NumTriangles;
			uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(NumTriangles, FBaseRecomputeTangentsPerTriangleShader::ThreadGroupSizeX);

			SCOPED_DRAW_EVENTF(RHICmdList, SkinTangents_PerTrianglePass, TEXT("%sTangentsTri  Mesh=%s, LOD=%d, Chunk=%d, IndexStart=%d Tri=%d BoneInfluenceType=%d UVPrecision=%d"),
				*RayTracingTag , *GetSkeletalMeshObjectName(Entry->GPUSkin), LODIndex, SectionIndex, DispatchData.IndexBufferOffsetValue, DispatchData.NumTriangles, Entry->BoneInfluenceType, bFullPrecisionUV);

			if (!GAllowDupedVertsForRecomputeTangents)
			{
#if WITH_EDITOR
				check(LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.DupVertData.Num() && LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.DupVertIndexData.Num());
#endif
				DispatchData.DuplicatedIndices = LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.DuplicatedVerticesIndexBuffer.VertexBufferSRV;
				DispatchData.DuplicatedIndicesIndices = LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.LengthAndIndexDuplicatedVerticesIndexBuffer.VertexBufferSRV;
			}

			if (!GRecomputeTangentsParallelDispatch)
			{
				// When triangle & vertex passes are interleaved, resource transition is needed in between.
				RHICmdList.Transition({
					DispatchData.GetActiveTangentRWBuffer()->UpdateAccessState(ERHIAccess::SRVCompute),
					StagingBuffer->UpdateAccessState(ERHIAccess::UAVCompute)
				});
			}

			INC_DWORD_STAT_BY(STAT_GPUSkinCache_NumTrianglesForRecomputeTangents, NumTriangles);

			const FRWBuffer& ShaderStagingBuffer = GRecomputeTangentsParallelDispatch ? DispatchData.GetIntermediateAccumulatedTangentBuffer()->Buffer : StagingBuffer->Buffer;

			FRHIComputeShader* ShaderRHI = Shader.GetComputeShader();
			SetComputePipelineState(RHICmdList, Shader.GetComputeShader());

			SetShaderParametersLegacyCS(RHICmdList, Shader, Entry, DispatchData, ShaderStagingBuffer);
			DispatchComputeShader(RHICmdList, Shader.GetShader(), ThreadGroupCountValue, 1, 1);
			UnsetShaderParametersLegacyCS(RHICmdList, Shader);

			IncrementDispatchCounter(RHICmdList);
		}
	}
	else
	{
		SCOPED_DRAW_EVENTF(RHICmdList, SkinTangents_PerVertexPass, TEXT("%sTangentsVertex Mesh=%s, LOD=%d, Chunk=%d, InputStreamStart=%d, OutputStreamStart=%d, Vert=%d"),
			*RayTracingTag, *GetSkeletalMeshObjectName(Entry->GPUSkin), LODIndex, SectionIndex, DispatchData.InputStreamStart, DispatchData.OutputStreamStart, DispatchData.NumVertices);
		//#todo-gpuskin Feature level?
		auto* GlobalShaderMap = GetGlobalShaderMap(GetFeatureLevel());
		TShaderMapRef<FRecomputeTangentsPerVertexPassCS<0>> ComputeShader0(GlobalShaderMap);
		TShaderMapRef<FRecomputeTangentsPerVertexPassCS<1>> ComputeShader1(GlobalShaderMap);
		TShaderRef<FBaseRecomputeTangentsPerVertexShader> ComputeShader;
		if (DispatchData.Section->RecomputeTangentsVertexMaskChannel < ESkinVertexColorChannel::None)
			ComputeShader = ComputeShader1;
		else
			ComputeShader = ComputeShader0;

		uint32 VertexCount = DispatchData.NumVertices;
		uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(VertexCount, ComputeShader->ThreadGroupSizeX);

		if (!GRecomputeTangentsParallelDispatch)
		{
			// When triangle & vertex passes are interleaved, resource transition is needed in between.
			RHICmdList.Transition({
				DispatchData.GetTangentRWBuffer()->UpdateAccessState(ERHIAccess::UAVCompute),
				StagingBuffer->UpdateAccessState(ERHIAccess::UAVCompute)
				});
		}

		SetComputePipelineState(RHICmdList, ComputeShader.GetComputeShader());

		SetShaderParametersLegacyCS(RHICmdList, ComputeShader, Entry, DispatchData, GRecomputeTangentsParallelDispatch ? DispatchData.GetIntermediateAccumulatedTangentBuffer()->Buffer : StagingBuffer->Buffer);
		DispatchComputeShader(RHICmdList, ComputeShader.GetShader(), ThreadGroupCountValue, 1, 1);
		UnsetShaderParametersLegacyCS(RHICmdList, ComputeShader);

		IncrementDispatchCounter(RHICmdList);
	}
}

FGPUSkinCache::FRWBuffersAllocation* FGPUSkinCache::TryAllocBuffer(uint32 NumVertices, bool WithTangnents, bool UseIntermediateTangents, uint32 NumTriangles, FRHICommandList& RHICmdList, const FName& OwnerName)
{
	uint64 MaxSizeInBytes = (uint64)(GSkinCacheSceneMemoryLimitInMB * 1024.0f * 1024.0f);
	uint64 RequiredMemInBytes = FRWBuffersAllocation::CalculateRequiredMemory(NumVertices, WithTangnents, UseIntermediateTangents, NumTriangles);
	if (bRequiresMemoryLimit && UsedMemoryInBytes + RequiredMemInBytes >= MaxSizeInBytes)
	{
		ExtraRequiredMemory += RequiredMemInBytes;

		// Can't fit
		return nullptr;
	}

	FRWBuffersAllocation* NewAllocation = new FRWBuffersAllocation(NumVertices, WithTangnents, UseIntermediateTangents, NumTriangles, RHICmdList, OwnerName);
	Allocations.Add(NewAllocation);

	UsedMemoryInBytes += RequiredMemInBytes;
	INC_MEMORY_STAT_BY(STAT_GPUSkinCache_TotalMemUsed, RequiredMemInBytes);

	return NewAllocation;
}

DECLARE_GPU_STAT(GPUSkinCache);

void FGPUSkinCache::MakeBufferTransitions(FRHICommandList& RHICmdList, TArray<FSkinCacheRWBuffer*>& Buffers, ERHIAccess ToState)
{
	if (Buffers.Num() > 0)
	{
		// The tangent accumulation buffers are shared between sections so they can end up in the list more than once. We
		// need to make sure we don't issue multiple transitions for the same resource in a single call, the RHIs can't deal with that.
		Algo::Sort(Buffers);

		TArray<FRHITransitionInfo, SceneRenderingAllocator> UAVs;
		UAVs.Reserve(Buffers.Num());
		
		FSkinCacheRWBuffer* LastBuffer = nullptr;
		for (FSkinCacheRWBuffer* Buffer : Buffers)
		{
			if (Buffer == LastBuffer)
			{
				continue;
			}

			LastBuffer = Buffer;
			if ( EnumHasAnyFlags(ToState, ERHIAccess::UAVMask) || Buffer->AccessState != ToState)
			{
				UAVs.Add(Buffer->UpdateAccessState(ToState));
			}
		}
		RHICmdList.Transition(MakeArrayView(UAVs.GetData(), UAVs.Num()));
	}
}

void FGPUSkinCache::GetBufferUAVs(const TArray<FSkinCacheRWBuffer*>& InBuffers, TArray<FRHIUnorderedAccessView*>& OutUAVs)
{
	OutUAVs.Reset(InBuffers.Num());

	// It looks like BeginUAVOverlap wants to get in unique buffers, without any overlaps. Previously that worked out
	// because InBuffers was being filled with AddUnique, but that can add quite a bit of cost when there are many
	// skinned meshes to process here.
	//
	// Using a TSet instead of TArray with AddUnique improves things, but it still came up slower than just pushing
	// everything and dealing with duplicates separately.
	//
	// So here I've added a member to FSkinCacheRWBuffer that we can just use to check if it's already been visited. On
	// 64-bit, it's not increasing the struct size (since there's a trailing ERHIAccess/uint32 already, it gets padded
	// up to an 8 byte multiple).
	for (const FSkinCacheRWBuffer* Buffer : InBuffers)
	{
		Buffer->UniqueOpToken = 0;
	}
	for (const FSkinCacheRWBuffer* Buffer : InBuffers)
	{
		if (Buffer->UniqueOpToken == 0)
		{
			Buffer->UniqueOpToken = 1;
			OutUAVs.Add(Buffer->Buffer.UAV);
		}
	}
}

void FGPUSkinCache::DoDispatch(FRHICommandList& RHICmdList)
{
	int32 BatchCount = BatchDispatches.Num();
	INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumChunks, BatchCount);

	if (!BatchCount)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FGPUSkinCache::DoDispatch);

	bool bCapture = BatchCount > 0 && GNumDispatchesToCapture > 0;
	RenderCaptureInterface::FScopedCapture RenderCapture(bCapture, &RHICmdList, TEXT("GPUSkinCache"));
	GNumDispatchesToCapture -= bCapture ? 1 : 0;

	SCOPED_GPU_STAT(RHICmdList, GPUSkinCache);

	TArray<FSkinCacheRWBuffer*> BuffersToTransitionForSkinning;
	BuffersToTransitionForSkinning.Reserve(BatchCount * 2);
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GPUSkinCache_PrepareUpdateSkinning);

		for (int32 i = 0; i < BatchCount; ++i)
		{
			FDispatchEntry& DispatchItem = BatchDispatches[i];
			PrepareUpdateSkinning(DispatchItem.SkinCacheEntry, DispatchItem.Section, DispatchItem.SkinCacheEntry->DispatchData[DispatchItem.Section].RevisionNumber, &BuffersToTransitionForSkinning);

			// Clear the flag that this is queued for dispatch.
			DispatchItem.SkinCacheEntry->bQueuedForDispatch = false;
			DispatchItem.SkinCacheEntry->DispatchData[DispatchItem.Section].RevisionNumber = 0;
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MakeBufferTransitions);
			MakeBufferTransitions(RHICmdList, BuffersToTransitionForSkinning, ERHIAccess::UAVCompute);
		}
	}

	TArray<FSkinCacheRWBuffer*> BuffersToTransitionToRead;

	TArray<FRHIUnorderedAccessView*> SkinningBuffersToOverlap;
	GetBufferUAVs(BuffersToTransitionForSkinning, SkinningBuffersToOverlap);
	RHICmdList.BeginUAVOverlap(SkinningBuffersToOverlap);
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GPUSkinCache_UpdateSkinningBatches);
		SCOPED_DRAW_EVENT(RHICmdList, GPUSkinCache_UpdateSkinningBatches);

		auto* GlobalShaderMap = GetGlobalShaderMap(GetFeatureLevel());

		TArray<FSortedDispatchEntry> SortedDispatches;
		SortedDispatches.Reserve(BatchCount);
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BuildSortedDispatchList);

			for (int32 i = 0; i < BatchCount; ++i)
			{
				FDispatchEntry& DispatchItem = BatchDispatches[i];
				FGPUSkinCacheEntry* Entry = DispatchItem.SkinCacheEntry;
				int32 Section = DispatchItem.Section;
				FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[Section];

				if ((DispatchData.DispatchFlags & ((uint32)EGPUSkinCacheDispatchFlags::DispatchPrevPosition | (uint32)EGPUSkinCacheDispatchFlags::DispatchPosition)) != 0)
				{
					// For 'unlimited' bone indexes, we pass in the index and weight sizes via a shader parameter and so we
					// can re-use the same shader permutation as for 8-bit indexes.
					bool bUse16BitBoneIndex = Entry->bUse16BitBoneIndex;
					bool bUse16BitBoneWeight = Entry->bUse16BitBoneWeight;
					if (Entry->BoneInfluenceType == 2)
					{
						bUse16BitBoneIndex = bUse16BitBoneWeight = false;
					}

					FSortedDispatchEntry SortedEntry;

					SortedEntry.ShaderIndex = SKIN_CACHE_SHADER_IDX(
						static_cast<int32>(bUse16BitBoneWeight),
						static_cast<int32>(bUse16BitBoneIndex),
						Entry->BoneInfluenceType,
						DispatchData.SkinType);
					SortedEntry.BatchIndex = i;

					SortedDispatches.Add(SortedEntry);
				}
			}

			Algo::Sort(SortedDispatches,
				[](const FSortedDispatchEntry& A, const FSortedDispatchEntry& B)
				{
					if (A.ShaderIndex != B.ShaderIndex)
					{
						return A.ShaderIndex < B.ShaderIndex;
					}
			return A.BatchIndex < B.BatchIndex;
				});
		}

		int32 LastShaderIndex = -1;
		TShaderRef<FBaseGPUSkinCacheCS> Shader;

		TShaderRef<FBaseGPUSkinCacheCS> AllShaders[64];
#define SKIN_CACHE_SHADER(_SHADER_IDX_) static_assert(_SHADER_IDX_ < 64); AllShaders[_SHADER_IDX_] = TShaderMapRef<TGPUSkinCacheCS<_SHADER_IDX_>>(GlobalShaderMap);
		SKIN_CACHE_SHADER_ALL();
#undef SKIN_CACHE_SHADER

		int32 SortedCount = SortedDispatches.Num();
		for (int32 i = 0; i < SortedCount; ++i)
		{
			const FSortedDispatchEntry& SortedEntry = SortedDispatches[i];
			if (SortedEntry.ShaderIndex != LastShaderIndex)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ChangeShader);

				if (Shader.IsValid())
				{
					UnsetShaderParametersLegacyCS(RHICmdList, Shader);
				}

				LastShaderIndex = SortedEntry.ShaderIndex;
				Shader = AllShaders[SortedEntry.ShaderIndex];

				check(Shader.IsValid());

				SetComputePipelineState(RHICmdList, Shader.GetComputeShader());
			}

			// This is pulled from FGPUSkinCache::DispatchUpdateSkinning() below, but inlined so we can set the
			// shader only when it changes. Not sure if it's worth pulling out the common bits into a shared function.

			FDispatchEntry& DispatchEntry = BatchDispatches[SortedEntry.BatchIndex];
			FGPUSkinCacheEntry* Entry = DispatchEntry.SkinCacheEntry;
			FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[DispatchEntry.Section];
			FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = DispatchData.SourceVertexFactory->GetShaderData();
			const TCHAR* RayTracingTag = (Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""));

			SCOPED_DRAW_EVENTF(RHICmdList, SkinCacheDispatch,
				TEXT("%sSkinning%d%d%d%d Mesh=%s LOD=%d Chunk=%d InStreamStart=%d OutStart=%d Vert=%d Morph=%d/%d"),
				RayTracingTag, (int32)Entry->bUse16BitBoneIndex, (int32)Entry->bUse16BitBoneWeight, (int32)Entry->BoneInfluenceType, DispatchData.SkinType, *GetSkeletalMeshObjectName(Entry->GPUSkin), Entry->LOD,
				DispatchData.SectionIndex, DispatchData.InputStreamStart, DispatchData.OutputStreamStart, DispatchData.NumVertices, Entry->MorphBuffer != 0, DispatchData.MorphBufferOffset);

			uint32 VertexCountAlign64 = FMath::DivideAndRoundUp(DispatchData.NumVertices, (uint32)64);

			if ((DispatchData.DispatchFlags & (uint32)EGPUSkinCacheDispatchFlags::DispatchPrevPosition) != 0)
			{
				const FVertexBufferAndSRV& PrevBoneBuffer = ShaderData.GetBoneBufferForReading(true);

				SetShaderParametersLegacyCS(
					RHICmdList,
					Shader,
					PrevBoneBuffer,
					Entry,
					DispatchData,
					DispatchData.GetPreviousPositionRWBuffer()->Buffer.UAV,
					DispatchData.GetActiveTangentRWBuffer() ? DispatchData.GetActiveTangentRWBuffer()->Buffer.UAV.GetReference() : nullptr
				);

				INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, VertexCountAlign64 * 64);
				RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);

				IncrementDispatchCounter(RHICmdList);
				BuffersToTransitionToRead.Add(DispatchData.GetPreviousPositionRWBuffer());
			}

			if ((DispatchData.DispatchFlags & (uint32)EGPUSkinCacheDispatchFlags::DispatchPosition) != 0)
			{
				const FVertexBufferAndSRV& BoneBuffer = ShaderData.GetBoneBufferForReading(false);

				SetShaderParametersLegacyCS(
					RHICmdList,
					Shader,
					BoneBuffer,
					Entry,
					DispatchData,
					DispatchData.GetPositionRWBuffer()->Buffer.UAV,
					DispatchData.GetActiveTangentRWBuffer() ? DispatchData.GetActiveTangentRWBuffer()->Buffer.UAV.GetReference() : nullptr
				);

				INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, VertexCountAlign64 * 64);
				RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);

				IncrementDispatchCounter(RHICmdList);
				BuffersToTransitionToRead.Add(DispatchData.GetPositionRWBuffer());
			}

			BuffersToTransitionToRead.Add(DispatchData.GetTangentRWBuffer());
			check(DispatchData.PreviousPositionBuffer != DispatchData.PositionBuffer);
		}

		if (Shader.IsValid())
		{
			UnsetShaderParametersLegacyCS(RHICmdList, Shader);
		}
	}
	RHICmdList.EndUAVOverlap(SkinningBuffersToOverlap);

	// Do necessary buffer transitions before recomputing tangents
	TArray<FSkinCacheRWBuffer*> BuffersToSRVForRecomputeTangents;
	TArray<FSkinCacheRWBuffer*> IntermediateAccumulatedTangentBuffers;
	for (int32 i = 0; i < BatchCount; ++i)
	{
		FDispatchEntry& DispatchItem = BatchDispatches[i];
		FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = DispatchItem.SkinCacheEntry->DispatchData[DispatchItem.Section];
		if (DispatchData.IndexBuffer)
		{
			BuffersToSRVForRecomputeTangents.Add(DispatchData.GetPositionRWBuffer());
			BuffersToSRVForRecomputeTangents.Add(DispatchData.GetActiveTangentRWBuffer());
			if (GRecomputeTangentsParallelDispatch)
			{
				IntermediateAccumulatedTangentBuffers.Add(DispatchData.GetIntermediateAccumulatedTangentBuffer());
			}
			BuffersToTransitionToRead.Add(DispatchData.GetPositionRWBuffer());
		}	
	}
	MakeBufferTransitions(RHICmdList, BuffersToSRVForRecomputeTangents, ERHIAccess::SRVCompute);
	MakeBufferTransitions(RHICmdList, IntermediateAccumulatedTangentBuffers, ERHIAccess::UAVCompute);

	TArray<FRHIUnorderedAccessView*> IntermediateAccumulatedTangentBuffersToOverlap;
	GetBufferUAVs(IntermediateAccumulatedTangentBuffers, IntermediateAccumulatedTangentBuffersToOverlap);
	RHICmdList.BeginUAVOverlap(IntermediateAccumulatedTangentBuffersToOverlap);
	{
		SCOPED_DRAW_EVENT(RHICmdList, GPUSkinCache_RecomputeTangentsBatches);
		FSkinCacheRWBuffer* StagingBuffer = nullptr;
		for (int32 i = 0; i < BatchCount; ++i)
		{
			FDispatchEntry& DispatchItem = BatchDispatches[i];
			if (DispatchItem.SkinCacheEntry->DispatchData[DispatchItem.Section].IndexBuffer)
			{
				DispatchUpdateSkinTangents(RHICmdList, DispatchItem.SkinCacheEntry, DispatchItem.Section, StagingBuffer, true);
				if (!GRecomputeTangentsParallelDispatch)
				{
					// When parallel dispatching is off, triangle pass and vertex pass are dispatched interleaved.
					DispatchUpdateSkinTangents(RHICmdList, DispatchItem.SkinCacheEntry, DispatchItem.Section, StagingBuffer, false);
				}
			}
		}
		if (GRecomputeTangentsParallelDispatch)
		{
			// Do necessary buffer transitions before vertex pass dispatches
			TArray<FSkinCacheRWBuffer*> TangentBuffers;
			for (int32 i = 0; i < BatchCount; ++i)
			{
				FDispatchEntry& DispatchItem = BatchDispatches[i];
				FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = DispatchItem.SkinCacheEntry->DispatchData[DispatchItem.Section];
				TangentBuffers.Add(DispatchData.GetTangentRWBuffer());
				BuffersToTransitionToRead.Add(DispatchData.GetTangentRWBuffer());
			}
			MakeBufferTransitions(RHICmdList, TangentBuffers, ERHIAccess::UAVCompute);
			MakeBufferTransitions(RHICmdList, IntermediateAccumulatedTangentBuffers, ERHIAccess::UAVCompute);
		
			TArray<FRHIUnorderedAccessView*> TangentBuffersToOverlap;
			GetBufferUAVs(TangentBuffers, TangentBuffersToOverlap);
			RHICmdList.BeginUAVOverlap(TangentBuffersToOverlap);
			for (int32 i = 0; i < BatchCount; ++i)
			{
				FDispatchEntry& DispatchItem = BatchDispatches[i];
				if (DispatchItem.SkinCacheEntry->DispatchData[DispatchItem.Section].IndexBuffer)
				{
					DispatchUpdateSkinTangents(RHICmdList, DispatchItem.SkinCacheEntry, DispatchItem.Section, StagingBuffer, false);
				}
			}
			RHICmdList.EndUAVOverlap(TangentBuffersToOverlap);
		}
	}
	RHICmdList.EndUAVOverlap(IntermediateAccumulatedTangentBuffersToOverlap);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateVertexFactoryDeclarations);

		for (int32 i = 0; i < BatchCount; ++i)
		{
			FDispatchEntry& DispatchItem = BatchDispatches[i];
			DispatchItem.SkinCacheEntry->UpdateVertexFactoryDeclaration(RHICmdList, DispatchItem.Section);
		}
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TransitionAllToReadable);
		TransitionAllToReadable(RHICmdList, BuffersToTransitionToRead);
	}

#if RHI_RAYTRACING
	if (IsGPUSkinCacheRayTracingSupported())
	{
		for (FGPUSkinCacheEntry* SkinCacheEntry : PendingProcessRTGeometryEntries)
		{
			ProcessRayTracingGeometryToUpdate(RHICmdList, SkinCacheEntry);
		}

		PendingProcessRTGeometryEntries.Reset();
	}
#endif

	BatchDispatches.Reset();
}

void FGPUSkinCache::DoDispatch(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* SkinCacheEntry, int32 Section, int32 RevisionNumber)
{
	RenderCaptureInterface::FScopedCapture RenderCapture(GNumDispatchesToCapture > 0, &RHICmdList, TEXT("GPUSkinCache"));
	GNumDispatchesToCapture = FMath::Max(GNumDispatchesToCapture - 1, 0);

	SCOPED_GPU_STAT(RHICmdList, GPUSkinCache);

	INC_DWORD_STAT(STAT_GPUSkinCache_TotalNumChunks);

	TArray<FSkinCacheRWBuffer*> BuffersToTransitionToRead;

	TArray<FSkinCacheRWBuffer*> BuffersToTransitionForSkinning;
	PrepareUpdateSkinning(SkinCacheEntry, Section, RevisionNumber, &BuffersToTransitionForSkinning);
	MakeBufferTransitions(RHICmdList, BuffersToTransitionForSkinning, ERHIAccess::UAVCompute);

	TArray<FRHIUnorderedAccessView*> SkinningBuffersToOverlap;
	GetBufferUAVs(BuffersToTransitionForSkinning, SkinningBuffersToOverlap);
	RHICmdList.BeginUAVOverlap(SkinningBuffersToOverlap);
	{
		DispatchUpdateSkinning(RHICmdList, SkinCacheEntry, Section, RevisionNumber, BuffersToTransitionToRead);
	}
	RHICmdList.EndUAVOverlap(SkinningBuffersToOverlap);

	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = SkinCacheEntry->DispatchData[Section];
	if (DispatchData.IndexBuffer)
	{
		RHICmdList.Transition({
			DispatchData.GetPositionRWBuffer()->UpdateAccessState(ERHIAccess::SRVCompute),
			DispatchData.GetActiveTangentRWBuffer()->UpdateAccessState(ERHIAccess::SRVCompute)
		});
		if (GRecomputeTangentsParallelDispatch)
		{
			RHICmdList.Transition(DispatchData.GetIntermediateAccumulatedTangentBuffer()->UpdateAccessState(ERHIAccess::UAVCompute));
		}
		BuffersToTransitionToRead.Add(DispatchData.GetPositionRWBuffer());

		FSkinCacheRWBuffer* StagingBuffer = nullptr;
		DispatchUpdateSkinTangents(RHICmdList, SkinCacheEntry, Section, StagingBuffer, true);
		if (GRecomputeTangentsParallelDispatch)
		{
			RHICmdList.Transition({
				DispatchData.GetTangentRWBuffer()->UpdateAccessState(ERHIAccess::UAVCompute),
				DispatchData.GetIntermediateAccumulatedTangentBuffer()->UpdateAccessState(ERHIAccess::UAVCompute)
			});
		}
		DispatchUpdateSkinTangents(RHICmdList, SkinCacheEntry, Section, StagingBuffer, false);
	}

	SkinCacheEntry->UpdateVertexFactoryDeclaration(RHICmdList, Section);

	TransitionAllToReadable(RHICmdList, BuffersToTransitionToRead);
}

bool FGPUSkinCache::ProcessEntry(
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
	FVector3f WorldScale,
	uint32 RevisionNumber, 
	int32 Section,
	int32 LODIndex,
	bool bRecreating,
	FGPUSkinCacheEntry*& InOutEntry
	)
{
	INC_DWORD_STAT(STAT_GPUSkinCache_NumSectionsProcessed);

	const int32 NumVertices = BatchElement.GetNumVertices();
	//#todo-gpuskin Check that stream 0 is the position stream
	const uint32 InputStreamStart = BatchElement.BaseVertexIndex;

	FSkeletalMeshRenderData& SkelMeshRenderData = Skin->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData.LODRenderData[LODIndex];

	if (FlushCounter < GGPUSkinCacheFlushCounter)
	{
		FlushCounter = GGPUSkinCacheFlushCounter;
		InvalidateAllEntries();
	}

	int32 RecomputeTangentsMode = GSkinCacheRecomputeTangents;
	bool bShouldRecomputeTangent = false;

	// IntermediateAccumulatedTangents buffer is needed if mesh has at least one section needing recomputing tangents.
	uint32 InterAccumTangentBufferSize = 0;
	uint32 CurrInterAccumTangentBufferOffset = 0;
	if (RecomputeTangentsMode > 0)
	{
		for (int32 i = 0; i < LodData.RenderSections.Num(); ++i)
		{			
			const FSkelMeshRenderSection& RenderSection = LodData.RenderSections[i];
			if (RecomputeTangentsMode == 1 || RenderSection.bRecomputeTangent)
			{
				bShouldRecomputeTangent = true;
				InterAccumTangentBufferSize += RenderSection.GetNumVertices();
				if (i < Section)
				{
					CurrInterAccumTangentBufferOffset += RenderSection.GetNumVertices();
				}
			}
		}
	}

	if (InOutEntry)
	{
		// If the LOD changed, the entry has to be invalidated
		if (!InOutEntry->IsValid(Skin, LODIndex))
		{
			Release(InOutEntry);
			InOutEntry = nullptr;
		}
		else
		{
			if (!InOutEntry->IsSectionValid(Section) || !InOutEntry->IsSourceFactoryValid(Section, VertexFactory))
			{
				// This section might not be valid yet, so set it up
				InOutEntry->SetupSection(Section, InOutEntry->PositionAllocation, &LodData.RenderSections[Section], MorphVertexBuffer, ClothVertexBuffer, NumVertices, InputStreamStart, 
											VertexFactory, TargetVertexFactory, CurrInterAccumTangentBufferOffset, SimData);
			}
		}
	}

	// Try to allocate a new entry
	if (!InOutEntry)
	{
		// If something caused the existing entry to be invalid, disable recreate logic for the rest of the function
		bRecreating = false;

		const bool WithTangents = true;
		int32 TotalNumVertices = VertexFactory->GetNumVertices();
		
		// IntermediateTangents buffer is needed if mesh has at least one section using vertex color as recompute tangents blending mask
		bool bEntryUseIntermediateTangents = false;
		if (bShouldRecomputeTangent)
		{
			for (const FSkelMeshRenderSection& RenderSection : LodData.RenderSections)
			{
				if (RenderSection.RecomputeTangentsVertexMaskChannel < ESkinVertexColorChannel::None)
				{
					bEntryUseIntermediateTangents = true;
					break;
				}
			}
		}

		FRWBuffersAllocation* NewPositionAllocation = TryAllocBuffer(TotalNumVertices, WithTangents, bEntryUseIntermediateTangents, InterAccumTangentBufferSize, RHICmdList, Skin->GetAssetPathName(LODIndex));
		if (!NewPositionAllocation)
		{
			if (GSkinCachePrintMemorySummary > 0)
			{
				const FString RayTracingTag = (Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""));
				uint64 RequiredMemInBytes = FRWBuffersAllocation::CalculateRequiredMemory(TotalNumVertices, WithTangents, bEntryUseIntermediateTangents, InterAccumTangentBufferSize);
				UE_LOG(LogSkinCache, Warning, TEXT("FGPUSkinCache::ProcessEntry%s failed to allocate %.3fMB for mesh %s LOD%d, extra required memory increased to %.3fMB"),
					*RayTracingTag, RequiredMemInBytes / MBSize, *GetSkeletalMeshObjectName(Skin), LODIndex, ExtraRequiredMemory / MBSize);
			}

			// Couldn't fit; caller will notify OOM
			return false;
		}

		InOutEntry = new FGPUSkinCacheEntry(this, Skin, NewPositionAllocation, LODIndex, Mode);
		InOutEntry->GPUSkin = Skin;

		InOutEntry->SetupSection(Section, NewPositionAllocation, &LodData.RenderSections[Section], MorphVertexBuffer, ClothVertexBuffer, NumVertices, InputStreamStart, 
									VertexFactory, TargetVertexFactory, CurrInterAccumTangentBufferOffset, SimData);
		Entries.Add(InOutEntry);
	}

	FGPUSkinCacheEntry::FSectionDispatchData& SectionDispatchData = InOutEntry->DispatchData[Section];

	const bool bMorph = MorphVertexBuffer && MorphVertexBuffer->SectionIds.Contains(Section);
	if (bMorph)
	{
		InOutEntry->MorphBuffer = MorphVertexBuffer->GetSRV();
		check(InOutEntry->MorphBuffer);

		const uint32 MorphStride = sizeof(FMorphGPUSkinVertex);

		// see GPU code "check(MorphStride == sizeof(float) * 6);"
		check(MorphStride == sizeof(float) * 6);

		SectionDispatchData.MorphBufferOffset = BatchElement.BaseVertexIndex;

		// weight buffer
		FSkinWeightVertexBuffer* WeightBuffer = Skin->GetSkinWeightVertexBuffer(LODIndex);
		uint32 WeightStride = WeightBuffer->GetConstantInfluencesVertexStride();
		SectionDispatchData.InputWeightStart = (WeightStride * BatchElement.BaseVertexIndex) / sizeof(float);
		InOutEntry->InputWeightStride = WeightStride;
		InOutEntry->InputWeightStreamSRV = WeightBuffer->GetDataVertexBuffer()->GetSRV();
	}

    if (ClothVertexBuffer)
    {
		FVertexBufferAndSRV ClothPositionAndNormalsBuffer;
		TSkeletalMeshVertexData<FVector3f> VertexAndNormalData(true);
        InOutEntry->ClothBuffer = ClothVertexBuffer->GetSRV();
        check(InOutEntry->ClothBuffer);

		if (SimData->Positions.Num() > 0)
		{
	        check(SimData->Positions.Num() == SimData->Normals.Num());
	        VertexAndNormalData.ResizeBuffer( 2 * SimData->Positions.Num() );

			FVector3f* Data = (FVector3f*)VertexAndNormalData.GetDataPointer();
	        uint32 Stride = VertexAndNormalData.GetStride();

	        // Copy the vertices into the buffer.
	        checkSlow(Stride*VertexAndNormalData.GetNumVertices() == sizeof(FVector3f) * 2 * SimData->Positions.Num());

			if (ClothVertexBuffer && ClothVertexBuffer->GetClothIndexMapping().Num() > Section)
			{
				const FClothBufferIndexMapping& ClothBufferIndexMapping = ClothVertexBuffer->GetClothIndexMapping()[Section];

				check(SimData->LODIndex != INDEX_NONE && SimData->LODIndex <= LODIndex);
				const uint32 ClothLODBias = (uint32)(LODIndex - SimData->LODIndex);

				const uint32 ClothBufferOffset = ClothBufferIndexMapping.MappingOffset + ClothBufferIndexMapping.LODBiasStride * ClothLODBias;

				// Set the buffer offset depending on whether enough deformer mapping data exists (RaytracingMinLOD/RaytracingLODBias/ClothLODBiasMode settings)
				const uint32 NumInfluences = NumVertices ? ClothBufferIndexMapping.LODBiasStride / NumVertices : 1;
				SectionDispatchData.ClothBufferOffset = (ClothBufferOffset + NumVertices * NumInfluences <= ClothVertexBuffer->GetNumVertices()) ?
					ClothBufferOffset :                     // If the offset is valid, set the calculated LODBias offset
					ClothBufferIndexMapping.MappingOffset;  // Otherwise fallback to a 0 ClothLODBias to prevent from reading pass the buffer (but still raytrace broken shadows/reflections/etc.)
			}

			for (int32 Index = 0; Index < SimData->Positions.Num(); Index++)
			{
				*(Data + Index * 2) = SimData->Positions[Index];
				*(Data + Index * 2 + 1) = SimData->Normals[Index];
			}

	        FResourceArrayInterface* ResourceArray = VertexAndNormalData.GetResourceArray();
	        check(ResourceArray->GetResourceDataSize() > 0);

	        FRHIResourceCreateInfo CreateInfo(TEXT("ClothPositionAndNormalsBuffer"), ResourceArray);
	        ClothPositionAndNormalsBuffer.VertexBufferRHI = RHICmdList.CreateVertexBuffer( ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
	        ClothPositionAndNormalsBuffer.VertexBufferSRV = RHICmdList.CreateShaderResourceView(ClothPositionAndNormalsBuffer.VertexBufferRHI, sizeof(FVector2f), PF_G32R32F);
			SectionDispatchData.ClothPositionsAndNormalsBuffer = ClothPositionAndNormalsBuffer.VertexBufferSRV;
		}
		else
		{
			UE_LOG(LogSkinCache, Error, TEXT("Cloth sim data is missing on mesh %s"), *GetSkeletalMeshObjectName(Skin));
		}

		SectionDispatchData.ClothBlendWeight = ClothBlendWeight;
		SectionDispatchData.ClothToLocal = ClothToLocal;
		SectionDispatchData.WorldScale = WorldScale;
    }
	SectionDispatchData.SkinType = ClothVertexBuffer && SectionDispatchData.ClothPositionsAndNormalsBuffer ? 2 : (bMorph ? 1 : 0);

	// Need to update the previous bone buffer pointer, so logic that checks if the bone buffers changed (FGPUSkinCache::FRWBufferTracker::Find)
	// doesn't invalidate the previous frame position data.  Recreating the render state will have generated new bone buffers.
	if (bRecreating)
	{
		FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = VertexFactory->GetShaderData();
		if (ShaderData.HasBoneBufferForReading(true))
		{
			SectionDispatchData.PositionTracker.UpdatePreviousBoneBuffer(ShaderData.GetBoneBufferForReading(true), VertexFactory->GetShaderData().GetRevisionNumber(true));
		}
	}

	if (bShouldBatchDispatches)
	{
		InOutEntry->bQueuedForDispatch = true;

		bool bFoundEntry = false;

		if (SectionDispatchData.RevisionNumber != 0)
		{
			// Check if the combo of skin cache entry and section index already exists, if so use the entry and update to latest revision number.
			SectionDispatchData.RevisionNumber = FMath::Max(InOutEntry->DispatchData[Section].RevisionNumber, RevisionNumber);
			bFoundEntry = true;
		}

		if (!bFoundEntry)
		{
			SectionDispatchData.RevisionNumber = RevisionNumber;
			BatchDispatches.Add({ InOutEntry, uint32(Section) });
		}
	}
	else
	{
		DoDispatch(RHICmdList, InOutEntry, Section, RevisionNumber);
	}

#if RHI_RAYTRACING
	if (!Skin->ShouldUseSeparateSkinCacheEntryForRayTracing() || Mode == EGPUSkinCacheEntryMode::RayTracing)
	{
		// This is a RT skin cache entry
		PendingProcessRTGeometryEntries.Add(InOutEntry);
	}
#endif

	return true;
}

bool FGPUSkinCache::IsGPUSkinCacheRayTracingSupported()
{
#if RHI_RAYTRACING
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.SupportSkeletalMeshes"));
	static const bool SupportSkeletalMeshes = CVar->GetInt() != 0;
	return IsRayTracingAllowed() && SupportSkeletalMeshes && GEnableGPUSkinCache;
#else
	return false;
#endif
}

#if RHI_RAYTRACING

void FGPUSkinCache::ProcessRayTracingGeometryToUpdate(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* SkinCacheEntry)
{
	if (IsGPUSkinCacheRayTracingSupported() && SkinCacheEntry && SkinCacheEntry->GPUSkin && SkinCacheEntry->GPUSkin->bSupportRayTracing)
	{
 		TArray<FBufferRHIRef> VertexBuffers;
 		SkinCacheEntry->GetRayTracingSegmentVertexBuffers(VertexBuffers);

		const int32 LODIndex = SkinCacheEntry->LOD;
		FSkeletalMeshRenderData& SkelMeshRenderData = SkinCacheEntry->GPUSkin->GetSkeletalMeshRenderData();
		check(LODIndex < SkelMeshRenderData.LODRenderData.Num());
		FSkeletalMeshLODRenderData& LODModel = SkelMeshRenderData.LODRenderData[LODIndex];

 		SkinCacheEntry->GPUSkin->UpdateRayTracingGeometry(RHICmdList, LODModel, LODIndex, VertexBuffers);
	}
}

#endif

void FGPUSkinCache::BeginBatchDispatch()
{
	bShouldBatchDispatches = true;
	DispatchCounter = 0;
}

void FGPUSkinCache::EndBatchDispatch()
{
	bShouldBatchDispatches = false;
}

void FGPUSkinCache::Release(FGPUSkinCacheEntry*& SkinCacheEntry)
{
	if (SkinCacheEntry)
	{
		FGPUSkinCache* SkinCache = SkinCacheEntry->SkinCache;
		check(SkinCache);
		SkinCache->PendingProcessRTGeometryEntries.Remove(SkinCacheEntry);

		if (SkinCacheEntry->bQueuedForDispatch)
		{
			for (int32 Index = 0; Index < SkinCache->BatchDispatches.Num(); )
			{
				if (SkinCache->BatchDispatches[Index].SkinCacheEntry == SkinCacheEntry)
				{
					SkinCache->BatchDispatches.RemoveAtSwap(Index);

					// Continue to search for other sections associated with this skin cache entry.
				}
				else
				{
					++Index;
				}
			}
			SkinCacheEntry->bQueuedForDispatch = false;
		}

		ReleaseSkinCacheEntry(SkinCacheEntry);
		SkinCacheEntry = nullptr;
	}
}

void FGPUSkinCache::GetShaderVertexStreams(
	const FGPUSkinCacheEntry* Entry, 
	int32 Section,
	const FGPUSkinPassthroughVertexFactory* VertexFactory,
	FVertexInputStreamArray& VertexStreams)
{
	INC_DWORD_STAT(STAT_GPUSkinCache_NumSetVertexStreams);
	check(Entry);
	check(Entry->IsSectionValid(Section));
	check(Entry->SkinCache);

	FGPUSkinCacheEntry::FSectionDispatchData const& DispatchData = Entry->DispatchData[Section];

	const int32 PositionStreamIndex = VertexFactory->GetAttributeStreamIndex(FGPUSkinPassthroughVertexFactory::EVertexAtttribute::VertexPosition);
	check(PositionStreamIndex > -1);
	VertexStreams.Add(FVertexInputStream(PositionStreamIndex, 0, DispatchData.GetPositionRWBuffer()->Buffer.Buffer));

	const int32 TangentStreamIndex = VertexFactory->GetAttributeStreamIndex(FGPUSkinPassthroughVertexFactory::EVertexAtttribute::VertexTangent);
	if (TangentStreamIndex > -1 && DispatchData.GetTangentRWBuffer())
	{
		VertexStreams.Add(FVertexInputStream(TangentStreamIndex, 0, DispatchData.GetTangentRWBuffer()->Buffer.Buffer));
	}
}

void FGPUSkinCache::PrepareUpdateSkinning(FGPUSkinCacheEntry* Entry, int32 Section, uint32 RevisionNumber, TArray<FSkinCacheRWBuffer*>* OverlappedUAVs)
{
	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[Section];
	FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = DispatchData.SourceVertexFactory->GetShaderData();

	const FVertexBufferAndSRV& BoneBuffer = ShaderData.GetBoneBufferForReading(false);
	const FVertexBufferAndSRV& PrevBoneBuffer = ShaderData.GetBoneBufferForReading(true);

	uint32 CurrentRevision = ShaderData.GetRevisionNumber(false);
	uint32 PreviousRevision = ShaderData.GetRevisionNumber(true);

	DispatchData.DispatchFlags = 0;

	auto BufferUpdate = [&DispatchData, OverlappedUAVs](
		FSkinCacheRWBuffer*& PositionBuffer,
		const FVertexBufferAndSRV& BoneBuffer, 
		uint32 Revision,
		const FVertexBufferAndSRV& PrevBoneBuffer,
		uint32 PrevRevision,
		uint32 UpdateFlag
		)
	{
		PositionBuffer = DispatchData.PositionTracker.Find(BoneBuffer, Revision);
		if (!PositionBuffer)
		{
			PositionBuffer = DispatchData.PositionTracker.Advance(BoneBuffer, Revision, PrevBoneBuffer, PrevRevision);
			check(PositionBuffer);

			DispatchData.DispatchFlags |= UpdateFlag;

			if (OverlappedUAVs)
			{
				(*OverlappedUAVs).Add(PositionBuffer);
			}
		}
	};

	BufferUpdate(
		DispatchData.PreviousPositionBuffer,
		PrevBoneBuffer,
		PreviousRevision,
		BoneBuffer,
		CurrentRevision,
		(uint32)EGPUSkinCacheDispatchFlags::DispatchPrevPosition
		);

	BufferUpdate(
		DispatchData.PositionBuffer, 
		BoneBuffer, 
		CurrentRevision, 
		PrevBoneBuffer, 
		PreviousRevision, 
		(uint32)EGPUSkinCacheDispatchFlags::DispatchPosition
		);

	DispatchData.TangentBuffer = DispatchData.PositionTracker.GetTangentBuffer();
	DispatchData.IntermediateTangentBuffer = DispatchData.PositionTracker.GetIntermediateTangentBuffer();
	DispatchData.IntermediateAccumulatedTangentBuffer = DispatchData.PositionTracker.GetIntermediateAccumulatedTangentBuffer();

	if (OverlappedUAVs && DispatchData.DispatchFlags != 0 && DispatchData.GetActiveTangentRWBuffer())
	{
		(*OverlappedUAVs).Add(DispatchData.GetActiveTangentRWBuffer());
	}

	check(DispatchData.PreviousPositionBuffer != DispatchData.PositionBuffer);
}

void FGPUSkinCache::DispatchUpdateSkinning(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* Entry, int32 Section, uint32 RevisionNumber, TArray<FSkinCacheRWBuffer*>& BuffersToTransitionToRead)
{
	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[Section];
	FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = DispatchData.SourceVertexFactory->GetShaderData();
	const TCHAR* RayTracingTag = (Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""));

	SCOPED_DRAW_EVENTF(RHICmdList, SkinCacheDispatch,
		TEXT("%sSkinning%d%d%d%d Mesh=%s LOD=%d Chunk=%d InStreamStart=%d OutStart=%d Vert=%d Morph=%d/%d"),
		RayTracingTag, (int32)Entry->bUse16BitBoneIndex, (int32)Entry->bUse16BitBoneWeight, (int32)Entry->BoneInfluenceType, DispatchData.SkinType, *GetSkeletalMeshObjectName(Entry->GPUSkin), Entry->LOD,
		DispatchData.SectionIndex, DispatchData.InputStreamStart, DispatchData.OutputStreamStart, DispatchData.NumVertices, Entry->MorphBuffer != 0, DispatchData.MorphBufferOffset);
	auto* GlobalShaderMap = GetGlobalShaderMap(GetFeatureLevel());

	// For 'unlimited' bone indexes, we pass in the index and weight sizes via a shader parameter and so we
	// can re-use the same shader permutation as for 8-bit indexes.
	bool bUse16BitBoneIndex = Entry->bUse16BitBoneIndex;
	bool bUse16BitBoneWeight = Entry->bUse16BitBoneWeight;
	if (Entry->BoneInfluenceType == 2)
	{
		bUse16BitBoneIndex = bUse16BitBoneWeight = false;
	}

	int32 ShaderIndex = SKIN_CACHE_SHADER_IDX(
		static_cast<int32>(bUse16BitBoneWeight),
		static_cast<int32>(bUse16BitBoneIndex),
		Entry->BoneInfluenceType,
		DispatchData.SkinType);

	TShaderRef<FBaseGPUSkinCacheCS> Shader;
	switch (ShaderIndex)
	{
#define SKIN_CACHE_SHADER(_SHADER_IDX_) case _SHADER_IDX_: Shader = TShaderMapRef<TGPUSkinCacheCS<_SHADER_IDX_>>(GlobalShaderMap); break;
		SKIN_CACHE_SHADER_ALL()
#undef SKIN_CACHE_SHADER
	}

	check(Shader.IsValid());

	const bool bDispatchPrevPosition = (DispatchData.DispatchFlags & (uint32)EGPUSkinCacheDispatchFlags::DispatchPrevPosition) != 0;
	const bool bDispatchPosition = (DispatchData.DispatchFlags & (uint32)EGPUSkinCacheDispatchFlags::DispatchPosition) != 0;

	if (bDispatchPrevPosition || bDispatchPosition)
	{
		uint32 VertexCountAlign64 = FMath::DivideAndRoundUp(DispatchData.NumVertices, (uint32)64);

		if (bDispatchPrevPosition)
		{
			const FVertexBufferAndSRV& PrevBoneBuffer = ShaderData.GetBoneBufferForReading(true);

			SetComputePipelineState(RHICmdList, Shader.GetComputeShader());

			SetShaderParametersLegacyCS(
				RHICmdList,
				Shader,
				PrevBoneBuffer,
				Entry,
				DispatchData,
				DispatchData.GetPreviousPositionRWBuffer()->Buffer.UAV,
				DispatchData.GetActiveTangentRWBuffer() ? DispatchData.GetActiveTangentRWBuffer()->Buffer.UAV : nullptr
			);

			INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, VertexCountAlign64 * 64);
			RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);

			IncrementDispatchCounter(RHICmdList);
			BuffersToTransitionToRead.Add(DispatchData.GetPreviousPositionRWBuffer());
		}

		if (bDispatchPosition)
		{
			const FVertexBufferAndSRV& BoneBuffer = ShaderData.GetBoneBufferForReading(false);

			SetComputePipelineState(RHICmdList, Shader.GetComputeShader());

			SetShaderParametersLegacyCS(
				RHICmdList,
				Shader,
				BoneBuffer,
				Entry,
				DispatchData,
				DispatchData.GetPositionRWBuffer()->Buffer.UAV,
				DispatchData.GetActiveTangentRWBuffer() ? DispatchData.GetActiveTangentRWBuffer()->Buffer.UAV : nullptr
			);

			INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, VertexCountAlign64 * 64);
			RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);


			IncrementDispatchCounter(RHICmdList);
			BuffersToTransitionToRead.Add(DispatchData.GetPositionRWBuffer());
		}

		UnsetShaderParametersLegacyCS(RHICmdList, Shader);
	}

	BuffersToTransitionToRead.Add(DispatchData.GetTangentRWBuffer());
	check(DispatchData.PreviousPositionBuffer != DispatchData.PositionBuffer);
}

void FGPUSkinCache::FRWBuffersAllocation::RemoveAllFromTransitionArray(TSet<FSkinCacheRWBuffer*>& InBuffersToTransition)
{
	for (uint32 i = 0; i < NUM_BUFFERS; i++)
	{
		FSkinCacheRWBuffer& RWBuffer = PositionBuffers[i];
		InBuffersToTransition.Remove(&RWBuffer);
		
		if (auto TangentBuffer = GetTangentBuffer())
		{
			InBuffersToTransition.Remove(TangentBuffer);
		}
		if (auto IntermediateTangentBuffer = GetIntermediateTangentBuffer())
		{
			InBuffersToTransition.Remove(IntermediateTangentBuffer);
		}
	}
}

void FGPUSkinCache::ReleaseSkinCacheEntry(FGPUSkinCacheEntry* SkinCacheEntry)
{
	FGPUSkinCache* SkinCache = SkinCacheEntry->SkinCache;

	for (FGPUSkinCacheEntry::FSectionDispatchData& SectionData : SkinCacheEntry->GetDispatchData())
	{
		SectionData.TargetVertexFactory->ResetVertexAttributes();
	}

	FRWBuffersAllocation* PositionAllocation = SkinCacheEntry->PositionAllocation;
	if (PositionAllocation)
	{
		uint64 RequiredMemInBytes = PositionAllocation->GetNumBytes();
		SkinCache->UsedMemoryInBytes -= RequiredMemInBytes;
		DEC_MEMORY_STAT_BY(STAT_GPUSkinCache_TotalMemUsed, RequiredMemInBytes);

		SkinCache->Allocations.Remove(PositionAllocation);

		delete PositionAllocation;

		SkinCacheEntry->PositionAllocation = nullptr;
	}

	SkinCache->Entries.RemoveSingleSwap(SkinCacheEntry, EAllowShrinking::No);
	delete SkinCacheEntry;
}

bool FGPUSkinCache::IsEntryValid(FGPUSkinCacheEntry* SkinCacheEntry, int32 Section)
{
	return SkinCacheEntry->IsSectionValid(Section);
}

const FSkinBatchVertexFactoryUserData* FGPUSkinCache::GetVertexFactoryUserData(FGPUSkinCacheEntry* Entry, int32 Section)
{
	return Entry != nullptr ? &Entry->BatchElementsUserData[Section] : nullptr;
}

void FGPUSkinCache::InvalidateAllEntries()
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index]->LOD = -1;
	}

	for (int32 Index = 0; Index < StagingBuffers.Num(); ++Index)
	{
		StagingBuffers[Index].Release();
	}
	StagingBuffers.SetNum(0, EAllowShrinking::No);
	SET_MEMORY_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed, 0);
}

FGPUSkinCacheEntry const* FGPUSkinCache::GetSkinCacheEntry(uint32 ComponentId) const
{
	for (FGPUSkinCacheEntry* Entry : Entries)
	{
		if (Entry && Entry->GPUSkin && Entry->GPUSkin->GetComponentId() == ComponentId)
		{
			return Entry;
		}
	}
	return nullptr;
}

FRWBuffer* FGPUSkinCache::GetPositionBuffer(FGPUSkinCacheEntry const* Entry, uint32 SectionIndex)
{
	if (Entry)
	{
		FGPUSkinCacheEntry::FSectionDispatchData const& DispatchData = Entry->GetDispatchData()[SectionIndex];
		FSkinCacheRWBuffer* SkinCacheRWBuffer = DispatchData.PositionBuffer;
		return SkinCacheRWBuffer != nullptr ? &SkinCacheRWBuffer->Buffer : nullptr;
	}
	return nullptr;
}

FRWBuffer* FGPUSkinCache::GetPreviousPositionBuffer(FGPUSkinCacheEntry const* Entry, uint32 SectionIndex)
{
	if (Entry)
	{
		FGPUSkinCacheEntry::FSectionDispatchData const& DispatchData = Entry->GetDispatchData()[SectionIndex];
		FSkinCacheRWBuffer* SkinCacheRWBuffer = DispatchData.PreviousPositionBuffer;
		return SkinCacheRWBuffer != nullptr ? &SkinCacheRWBuffer->Buffer : nullptr;
	}
	return nullptr;
}

FRWBuffer* FGPUSkinCache::GetTangentBuffer(FGPUSkinCacheEntry const* Entry, uint32 SectionIndex)
{
	if (Entry)
	{
		FGPUSkinCacheEntry::FSectionDispatchData const& DispatchData = Entry->GetDispatchData()[SectionIndex];
		FSkinCacheRWBuffer* SkinCacheRWBuffer = DispatchData.TangentBuffer;
		return SkinCacheRWBuffer != nullptr ? &SkinCacheRWBuffer->Buffer : nullptr;
	}
	return nullptr;
}

uint32 FGPUSkinCache::GetUpdatedFrame(FGPUSkinCacheEntry const* Entry, uint32 SectionIndex)
{
	return Entry != nullptr ? Entry->GetDispatchData()[SectionIndex].UpdatedFrameNumber : 0;
}

void FGPUSkinCache::UpdateSkinWeightBuffer(FGPUSkinCacheEntry* Entry)
{
	if (Entry)
	{
		Entry->UpdateSkinWeightBuffer();
	}
}

void FGPUSkinCache::SetEntryGPUSkin(FGPUSkinCacheEntry* Entry, FSkeletalMeshObjectGPUSkin* Skin)
{
	if (Entry)
	{
		Entry->GPUSkin = Skin;
	}
}

void FGPUSkinCache::CVarSinkFunction()
{
	int32 NewGPUSkinCacheValue = CVarEnableGPUSkinCache.GetValueOnAnyThread() != 0;
	int32 NewRecomputeTangentsValue = CVarGPUSkinCacheRecomputeTangents.GetValueOnAnyThread();
	const float NewSceneMaxSizeInMb = CVarGPUSkinCacheSceneMemoryLimitInMB.GetValueOnAnyThread();
	const int32 NewNumTangentIntermediateBuffers = CVarGPUSkinNumTangentIntermediateBuffers.GetValueOnAnyThread();
	const bool NewSkipCompilingGPUSkinVF = CVarSkipCompilingGPUSkinVF.GetValueOnAnyThread();

	if (GEnableGPUSkinCacheShaders)
	{
		if (GIsRHIInitialized && IsGPUSkinCacheRayTracingSupported() && IsRayTracingEnabled())
		{
			// Skin cache is *required* for ray tracing.
			NewGPUSkinCacheValue = 1;
		}
	}
	else
	{
		NewGPUSkinCacheValue = 0;
		NewRecomputeTangentsValue = 0;
	}

	// We don't have GPU Skin VF shaders at all so we can't fallback to using GPU Skinning.
	if (NewSkipCompilingGPUSkinVF)
	{
		// If we had the skin cache enabled and we are turning it off.
		if (GEnableGPUSkinCache && (NewGPUSkinCacheValue == 0))
		{
			NewGPUSkinCacheValue = 1;
			UE_LOG(LogSkinCache, Warning, TEXT("Attemping to turn off the GPU Skin Cache, but we don't have GPU Skin VF shaders to fallback to (r.SkinCache.SkipCompilingGPUSkinVF=1).  Leaving skin cache turned on."));
		}
	}

	if (NewGPUSkinCacheValue != GEnableGPUSkinCache || NewRecomputeTangentsValue != GSkinCacheRecomputeTangents
		|| NewSceneMaxSizeInMb != GSkinCacheSceneMemoryLimitInMB || NewNumTangentIntermediateBuffers != GNumTangentIntermediateBuffers)
	{
		ENQUEUE_RENDER_COMMAND(DoEnableSkinCaching)(UE::RenderCommandPipe::SkeletalMesh,
			[NewRecomputeTangentsValue, NewGPUSkinCacheValue, NewSceneMaxSizeInMb, NewNumTangentIntermediateBuffers](FRHICommandList& RHICmdList)
		{
			GNumTangentIntermediateBuffers = FMath::Max(NewNumTangentIntermediateBuffers, 1);
			GEnableGPUSkinCache = NewGPUSkinCacheValue;
			GSkinCacheRecomputeTangents = NewRecomputeTangentsValue;
			GSkinCacheSceneMemoryLimitInMB = NewSceneMaxSizeInMb;
			++GGPUSkinCacheFlushCounter;
		}
		);
	}
}

FAutoConsoleVariableSink FGPUSkinCache::CVarSink(FConsoleCommandDelegate::CreateStatic(&CVarSinkFunction));

void FGPUSkinCache::IncrementDispatchCounter(FRHICommandList& RHICmdList)
{
	if (GSkinCacheMaxDispatchesPerCmdList > 0)
	{
		DispatchCounter++;
		if (DispatchCounter >= GSkinCacheMaxDispatchesPerCmdList)
		{
			//UE_LOG(LogSkinCache, Log, TEXT("SubmitCommandsHint issued after %d dispatches"), DispatchCounter);
			RHICmdList.SubmitCommandsHint();
			DispatchCounter = 0;
		}
	}
}

uint64 FGPUSkinCache::GetExtraRequiredMemoryAndReset()
{
	if (GSkinCachePrintMemorySummary == 2 || (GSkinCachePrintMemorySummary == 1 && ExtraRequiredMemory > 0))
	{
		PrintMemorySummary();
	}

	uint64 OriginalValue = ExtraRequiredMemory;
	ExtraRequiredMemory = 0;
	return OriginalValue;
}

void FGPUSkinCache::PrintMemorySummary() const
{
	UE_LOG(LogSkinCache, Display, TEXT("======= Skin Cache Memory Usage Summary ======="));

	uint64 TotalMemInBytes = 0;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		FGPUSkinCacheEntry* Entry = Entries[i];
		if (Entry)
		{
			FString RecomputeTangentSections = TEXT("");
			for (int32 DispatchIdx = 0; DispatchIdx < Entry->DispatchData.Num(); ++DispatchIdx)
			{
				const FGPUSkinCacheEntry::FSectionDispatchData& Data = Entry->DispatchData[DispatchIdx];
				if (Data.IndexBuffer)
				{
					if (RecomputeTangentSections.IsEmpty())
					{
						RecomputeTangentSections = TEXT("[Section]") + FString::FromInt(Data.SectionIndex);
					}
					else
					{
						RecomputeTangentSections = RecomputeTangentSections + TEXT("/") + FString::FromInt(Data.SectionIndex);
					}
				}
			}
			if (RecomputeTangentSections.IsEmpty())
			{
				RecomputeTangentSections = TEXT("Off");
			}

			const FString RayTracingTag = (Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""));
			uint64 MemInBytes = Entry->PositionAllocation ? Entry->PositionAllocation->GetNumBytes() : 0;
			uint64 TangentsInBytes = (Entry->PositionAllocation && Entry->PositionAllocation->GetTangentBuffer()) ? Entry->PositionAllocation->GetTangentBuffer()->Buffer.NumBytes : 0;
			uint64 IntermediateTangentsInBytes = (Entry->PositionAllocation && Entry->PositionAllocation->GetIntermediateTangentBuffer()) ? Entry->PositionAllocation->GetIntermediateTangentBuffer()->Buffer.NumBytes : 0;
			uint64 IntermediateAccumulatedTangentsInBytes = (Entry->PositionAllocation && Entry->PositionAllocation->GetIntermediateAccumulatedTangentBuffer()) ? Entry->PositionAllocation->GetIntermediateAccumulatedTangentBuffer()->Buffer.NumBytes : 0;

			UE_LOG(LogSkinCache, Display, TEXT("   SkinCacheEntry_%d: %sMesh=%s, LOD=%d, RecomputeTangent=%s, Mem=%.3fKB (Tangents=%.3fKB, InterTangents=%.3fKB, InterAccumTangents=%.3fKB)"), 
					i, *RayTracingTag, *GetSkeletalMeshObjectName(Entry->GPUSkin), Entry->LOD, *RecomputeTangentSections, 
					MemInBytes / 1024.f, TangentsInBytes / 1024.f, IntermediateTangentsInBytes / 1024.f, IntermediateAccumulatedTangentsInBytes / 1024.f);
			TotalMemInBytes += MemInBytes;
		}
	}
	ensure(TotalMemInBytes == UsedMemoryInBytes);

	uint64 MaxSizeInBytes = (uint64)(GSkinCacheSceneMemoryLimitInMB * MBSize);
	uint64 UnusedSizeInBytes = MaxSizeInBytes - UsedMemoryInBytes;

	UE_LOG(LogSkinCache, Display, TEXT("Used: %.3fMB"), UsedMemoryInBytes / MBSize);
	UE_LOG(LogSkinCache, Display, TEXT("Available: %.3fMB"), UnusedSizeInBytes / MBSize);
	UE_LOG(LogSkinCache, Display, TEXT("Total limit: %.3fMB"), GSkinCacheSceneMemoryLimitInMB);
	UE_LOG(LogSkinCache, Display, TEXT("Extra required: %.3fMB"), ExtraRequiredMemory / MBSize);
	UE_LOG(LogSkinCache, Display, TEXT("==============================================="));
}

FString FGPUSkinCache::GetSkeletalMeshObjectName(const FSkeletalMeshObjectGPUSkin* GPUSkin) const
{
	FString Name = TEXT("None");
	if (GPUSkin)
	{
#if !UE_BUILD_SHIPPING
		Name = GPUSkin->DebugName.ToString();
#endif // !UE_BUILD_SHIPPING
	}
	return Name;
}

FColor FGPUSkinCache::GetVisualizationDebugColor(const FName& GPUSkinCacheVisualizationMode, FGPUSkinCacheEntry* Entry, FGPUSkinCacheEntry* RayTracingEntry, uint32 SectionIndex)
{
	const FGPUSkinCacheVisualizationData& VisualizationData = GetGPUSkinCacheVisualizationData();
	if (VisualizationData.IsActive())
	{
		// Color coding should match DrawVisualizationInfoText function
		FGPUSkinCacheVisualizationData::FModeType ModeType = VisualizationData.GetActiveModeType();

		if (ModeType == FGPUSkinCacheVisualizationData::FModeType::Overview)
		{
			bool bRecomputeTangent = Entry && Entry->DispatchData[SectionIndex].IndexBuffer;
			return Entry ? 
				   (bRecomputeTangent ? GEngine->GPUSkinCacheVisualizationRecomputeTangentsColor.QuantizeRound() : GEngine->GPUSkinCacheVisualizationIncludedColor.QuantizeRound()) : 
				   GEngine->GPUSkinCacheVisualizationExcludedColor.QuantizeRound();
		}
		else if (ModeType == FGPUSkinCacheVisualizationData::FModeType::Memory)
		{
			uint64 MemoryInBytes = (Entry && Entry->PositionAllocation) ? Entry->PositionAllocation->GetNumBytes() : 0;
#if RHI_RAYTRACING
			if (RayTracingEntry && RayTracingEntry != Entry)
			{
				// Separate ray tracing entry
				MemoryInBytes += RayTracingEntry->PositionAllocation ? RayTracingEntry->PositionAllocation->GetNumBytes() : 0;
			}
#endif
			float MemoryInMB = MemoryInBytes / MBSize;

			return MemoryInMB < GEngine->GPUSkinCacheVisualizationLowMemoryThresholdInMB ? GEngine->GPUSkinCacheVisualizationLowMemoryColor.QuantizeRound() :
				  (MemoryInMB < GEngine->GPUSkinCacheVisualizationHighMemoryThresholdInMB ? GEngine->GPUSkinCacheVisualizationMidMemoryColor.QuantizeRound() : GEngine->GPUSkinCacheVisualizationHighMemoryColor.QuantizeRound());
		}
		else if (ModeType == FGPUSkinCacheVisualizationData::FModeType::RayTracingLODOffset)
		{
	#if RHI_RAYTRACING
			int32 LODOffset = (Entry && RayTracingEntry) ? (RayTracingEntry->LOD - Entry->LOD) : 0;
			check (LODOffset >= 0);
			const TArray<FLinearColor>& VisualizationColors = GEngine->GPUSkinCacheVisualizationRayTracingLODOffsetColors;
			if (VisualizationColors.Num() > 0)
			{
				int32 Index = VisualizationColors.IsValidIndex(LODOffset) ? LODOffset : (VisualizationColors.Num()-1);
				return VisualizationColors[Index].QuantizeRound();
			}
	#endif
		}
	}

	return FColor::White;
}

void FGPUSkinCache::DrawVisualizationInfoText(const FName& GPUSkinCacheVisualizationMode, FScreenMessageWriter& ScreenMessageWriter) const
{
	const FGPUSkinCacheVisualizationData& VisualizationData = GetGPUSkinCacheVisualizationData();
	if (VisualizationData.IsActive())
	{
		FGPUSkinCacheVisualizationData::FModeType ModeType = VisualizationData.GetActiveModeType();

		// Color coding should match GetVisualizationDebugColor function
		auto DrawText = [&ScreenMessageWriter](const FString& Message, const FColor& Color)
		{
			ScreenMessageWriter.DrawLine(FText::FromString(Message), 10, Color);
		};

		if (ModeType == FGPUSkinCacheVisualizationData::FModeType::Overview)
		{
			DrawText(TEXT("Skin Cache Visualization - Overview"), FColor::White);
			DrawText(TEXT("Non SK mesh"), FColor::White);
			DrawText(TEXT("SK Skin Cache Excluded"), GEngine->GPUSkinCacheVisualizationExcludedColor.QuantizeRound());
			DrawText(TEXT("SK Skin Cache Included"), GEngine->GPUSkinCacheVisualizationIncludedColor.QuantizeRound());
			DrawText(TEXT("SK Recompute Tangent ON"), GEngine->GPUSkinCacheVisualizationRecomputeTangentsColor.QuantizeRound());
		}
		else if (ModeType == FGPUSkinCacheVisualizationData::FModeType::Memory)
		{
			float UsedMemoryInMB = UsedMemoryInBytes / MBSize;
			float AvailableMemoryInMB = GSkinCacheSceneMemoryLimitInMB - UsedMemoryInMB;

			FString LowMemoryText = FString::Printf(TEXT("0 - %dMB"), GEngine->GPUSkinCacheVisualizationLowMemoryThresholdInMB);
			DrawText(TEXT("Skin Cache Visualization - Memory"), FColor::White);
			DrawText(FString::Printf(TEXT("Total Limit: %.2fMB"), GSkinCacheSceneMemoryLimitInMB), FColor::White);
			DrawText(FString::Printf(TEXT("Total Used: %.2fMB"), UsedMemoryInMB), FColor::White);
			DrawText(FString::Printf(TEXT("Total Available: %.2fMB"), AvailableMemoryInMB), FColor::White);
			DrawText(FString::Printf(TEXT("Low: < %.2fMB"), GEngine->GPUSkinCacheVisualizationLowMemoryThresholdInMB), GEngine->GPUSkinCacheVisualizationLowMemoryColor.QuantizeRound());
			DrawText(FString::Printf(TEXT("Mid: %.2f - %.2fMB"), GEngine->GPUSkinCacheVisualizationLowMemoryThresholdInMB, GEngine->GPUSkinCacheVisualizationHighMemoryThresholdInMB), GEngine->GPUSkinCacheVisualizationMidMemoryColor.QuantizeRound());
			DrawText(FString::Printf(TEXT("High: > %.2fMB"), GEngine->GPUSkinCacheVisualizationHighMemoryThresholdInMB), GEngine->GPUSkinCacheVisualizationHighMemoryColor.QuantizeRound());
		}
		else if (ModeType == FGPUSkinCacheVisualizationData::FModeType::RayTracingLODOffset)
		{
	#if RHI_RAYTRACING
			DrawText(TEXT("Skin Cache Visualization - RayTracingLODOffset"), FColor::White);
			const TArray<FLinearColor>& VisualizationColors = GEngine->GPUSkinCacheVisualizationRayTracingLODOffsetColors;
			for (int32 i = 0; i < VisualizationColors.Num(); ++i)
			{
				DrawText(FString::Printf(TEXT("RT_LOD == Raster_LOD %s %d"), (i > 0 ? TEXT("+") : TEXT("")), i), VisualizationColors[i].QuantizeRound());
			}
	#endif
		}
	}
}

#undef IMPLEMENT_SKIN_CACHE_SHADER_CLOTH
#undef IMPLEMENT_SKIN_CACHE_SHADER_ALL_SKIN_TYPES
#undef IMPLEMENT_SKIN_CACHE_SHADER