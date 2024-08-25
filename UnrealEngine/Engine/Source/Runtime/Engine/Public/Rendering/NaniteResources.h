// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/LowLevelMemTracker.h"
#include "GrowOnlySpanAllocator.h"
#include "IO/IoHash.h"
#include "UnifiedBuffer.h"
#include "RenderGraphDefinitions.h"
#include "SceneManagement.h"
#include "Materials/MaterialInterface.h"
#include "Serialization/BulkData.h"
#include "Misc/MemoryReadStream.h"
#include "NaniteDefinitions.h"
#include "NaniteInterface.h"
#include "Templates/DontCopy.h"
#include "VertexFactory.h"

/** Whether Nanite::FSceneProxy should store data and enable codepaths needed for debug rendering. */
#if PLATFORM_WINDOWS
#define NANITE_ENABLE_DEBUG_RENDERING (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)
#else
#define NANITE_ENABLE_DEBUG_RENDERING 0
#endif

DECLARE_STATS_GROUP( TEXT("Nanite"), STATGROUP_Nanite, STATCAT_Advanced );

DECLARE_GPU_STAT_NAMED_EXTERN(NaniteStreaming, TEXT("Nanite Streaming"));
DECLARE_GPU_STAT_NAMED_EXTERN(NaniteReadback, TEXT("Nanite Readback"));

LLM_DECLARE_TAG_API(Nanite, ENGINE_API);

class UStaticMesh;
class UBodySetup;
class FDistanceFieldVolumeData;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class FVertexFactory;
class FNaniteVertexFactory;

namespace UE::DerivedData { class FRequestOwner; }

namespace Nanite
{

struct FPackedHierarchyNode
{
	FVector4f		LODBounds[NANITE_MAX_BVH_NODE_FANOUT];
	
	struct
	{
		FVector3f	BoxBoundsCenter;
		uint32		MinLODError_MaxParentLODError;
	} Misc0[NANITE_MAX_BVH_NODE_FANOUT];

	struct
	{
		FVector3f	BoxBoundsExtent;
		uint32		ChildStartReference;
	} Misc1[NANITE_MAX_BVH_NODE_FANOUT];
	
	struct
	{
		uint32		ResourcePageIndex_NumPages_GroupPartSize;
	} Misc2[NANITE_MAX_BVH_NODE_FANOUT];
};


FORCEINLINE uint32 GetBits(uint32 Value, uint32 NumBits, uint32 Offset)
{
	uint32 Mask = (1u << NumBits) - 1u;
	return (Value >> Offset) & Mask;
}

FORCEINLINE void SetBits(uint32& Value, uint32 Bits, uint32 NumBits, uint32 Offset)
{
	uint32 Mask = (1u << NumBits) - 1u;
	check(Bits <= Mask);
	Mask <<= Offset;
	Value = (Value & ~Mask) | (Bits << Offset);
}


// Packed Cluster as it is used by the GPU
struct FPackedCluster
{
	// Members needed for rasterization
	uint32		NumVerts_PositionOffset;					// NumVerts:9, PositionOffset:23
	uint32		NumTris_IndexOffset;						// NumTris:8, IndexOffset: 24
	uint32		ColorMin;
	uint32		ColorBits_GroupIndex;						// R:4, G:4, B:4, A:4. (GroupIndex&0xFFFF) is for debug visualization only.

	FIntVector	PosStart;
	uint32		BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision;	// BitsPerIndex:4, PosPrecision: 5, PosBits:5.5.5, NormalPrecision: 4, TangentPrecision: 4
	
	// Members needed for culling
	FVector4f	LODBounds;									// LWC_TODO: Was FSphere, but that's now twice as big and won't work on GPU.

	FVector3f	BoxBoundsCenter;
	uint32		LODErrorAndEdgeLength;
	
	FVector3f	BoxBoundsExtent;
	uint32		Flags;

	// Members needed by materials
	uint32		AttributeOffset_BitsPerAttribute;				// AttributeOffset: 22, BitsPerAttribute: 10
	uint32		DecodeInfoOffset_HasTangents_NumUVs_ColorMode;	// DecodeInfoOffset: 22, bHasTangents: 1, NumUVs: 3, ColorMode: 2
	uint32		UVBitOffsets;									// Bit offsets of UV sets relative to beginning of UV data.
																// UV0 Offset: 8, UV1 Offset: 8, UV2 Offset: 8, UV3 Offset: 8
	uint32		PackedMaterialInfo;

	uint32		VertReuseBatchInfo[4];

	uint32		GetNumVerts() const						{ return GetBits(NumVerts_PositionOffset, 9, 0); }
	uint32		GetPositionOffset() const				{ return GetBits(NumVerts_PositionOffset, 23, 9); }

	uint32		GetNumTris() const						{ return GetBits(NumTris_IndexOffset, 8, 0); }
	uint32		GetIndexOffset() const					{ return GetBits(NumTris_IndexOffset, 24, 8); }

	uint32		GetBitsPerIndex() const					{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 3, 0) + 1; }
	int32		GetPosPrecision() const					{ return (int32)GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 6, 3) + NANITE_MIN_POSITION_PRECISION; }
	uint32		GetPosBitsX() const						{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 5, 9); }
	uint32		GetPosBitsY() const						{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 5, 14); }
	uint32		GetPosBitsZ() const						{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 5, 19); }
	uint32		GetNormalPrecision() const				{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 4, 24); }
	uint32		GetTangentPrecision() const				{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 4, 28); }

	uint32		GetAttributeOffset() const				{ return GetBits(AttributeOffset_BitsPerAttribute, 22, 0); }
	uint32		GetBitsPerAttribute() const				{ return GetBits(AttributeOffset_BitsPerAttribute, 10, 22); }
	
	void		SetNumVerts(uint32 NumVerts)			{ SetBits(NumVerts_PositionOffset, NumVerts, 9, 0); }
	void		SetPositionOffset(uint32 Offset)		{ SetBits(NumVerts_PositionOffset, Offset, 23, 9); }

	void		SetNumTris(uint32 NumTris)				{ SetBits(NumTris_IndexOffset, NumTris, 8, 0); }
	void		SetIndexOffset(uint32 Offset)			{ SetBits(NumTris_IndexOffset, Offset, 24, 8); }

	void		SetBitsPerIndex(uint32 BitsPerIndex)	{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, BitsPerIndex - 1, 3, 0); }
	void		SetPosPrecision(int32 Precision)		{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, uint32(Precision - NANITE_MIN_POSITION_PRECISION), 6, 3); }
	void		SetPosBitsX(uint32 NumBits)				{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 5, 9); }
	void		SetPosBitsY(uint32 NumBits)				{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 5, 14); }
	void		SetPosBitsZ(uint32 NumBits)				{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 5, 19); }
	void		SetNormalPrecision(uint32 NumBits)		{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 4, 24); }
	void		SetTangentPrecision(uint32 NumBits)		{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 4, 28); }

	void		SetAttributeOffset(uint32 Offset)		{ SetBits(AttributeOffset_BitsPerAttribute, Offset, 22, 0); }
	void		SetBitsPerAttribute(uint32 Bits)		{ SetBits(AttributeOffset_BitsPerAttribute, Bits, 10, 22); }

	void		SetDecodeInfoOffset(uint32 Offset)		{ SetBits(DecodeInfoOffset_HasTangents_NumUVs_ColorMode, Offset, 22, 0); }
	void		SetHasTangents(bool bHasTangents)		{ SetBits(DecodeInfoOffset_HasTangents_NumUVs_ColorMode, bHasTangents, 1, 22); }
	void		SetNumUVs(uint32 Num)					{ SetBits(DecodeInfoOffset_HasTangents_NumUVs_ColorMode, Num, 3, 23); }
	void		SetColorMode(uint32 Mode)				{ SetBits(DecodeInfoOffset_HasTangents_NumUVs_ColorMode, Mode, 1, 26); }

	void		SetColorBitsR(uint32 NumBits)			{ SetBits(ColorBits_GroupIndex, NumBits, 4, 0); }
	void		SetColorBitsG(uint32 NumBits)			{ SetBits(ColorBits_GroupIndex, NumBits, 4, 4); }
	void		SetColorBitsB(uint32 NumBits)			{ SetBits(ColorBits_GroupIndex, NumBits, 4, 8); }
	void		SetColorBitsA(uint32 NumBits)			{ SetBits(ColorBits_GroupIndex, NumBits, 4, 12); }

	void		SetGroupIndex(uint32 GroupIndex)		{ SetBits(ColorBits_GroupIndex, GroupIndex & 0xFFFFu, 16, 16); }

	void SetVertResourceBatchInfo(TArray<uint32>& BatchInfo, uint32 GpuPageOffset, uint32 NumMaterialRanges)
	{
		FMemory::Memzero(VertReuseBatchInfo, sizeof(VertReuseBatchInfo));
		if (NumMaterialRanges <= 3)
		{
			check(BatchInfo.Num() <= 4);
			FMemory::Memcpy(VertReuseBatchInfo, BatchInfo.GetData(), BatchInfo.Num() * sizeof(uint32));
		}
		else
		{
			check((GpuPageOffset & 0x3) == 0);
			VertReuseBatchInfo[0] = GpuPageOffset >> 2;
			VertReuseBatchInfo[1] = NumMaterialRanges;
		}
	}
};

struct FPageStreamingState
{
	uint32			BulkOffset;
	uint32			BulkSize;
	uint32			PageSize;
	uint32			DependenciesStart;
	uint16			DependenciesNum;
	uint8			MaxHierarchyDepth;
	uint8			Flags;
};

class FHierarchyFixup
{
public:
	FHierarchyFixup() {}

	FHierarchyFixup( uint32 InPageIndex, uint32 NodeIndex, uint32 ChildIndex, uint32 InClusterGroupPartStartIndex, uint32 PageDependencyStart, uint32 PageDependencyNum )
	{
		check(InPageIndex < NANITE_MAX_RESOURCE_PAGES);
		PageIndex = InPageIndex;

		check( NodeIndex < ( 1 << ( 32 - NANITE_MAX_HIERACHY_CHILDREN_BITS ) ) );
		check( ChildIndex < NANITE_MAX_HIERACHY_CHILDREN );
		check( InClusterGroupPartStartIndex < ( 1 << ( 32 - NANITE_MAX_CLUSTERS_PER_GROUP_BITS ) ) );
		HierarchyNodeAndChildIndex = ( NodeIndex << NANITE_MAX_HIERACHY_CHILDREN_BITS ) | ChildIndex;
		ClusterGroupPartStartIndex = InClusterGroupPartStartIndex;

		check(PageDependencyStart < NANITE_MAX_RESOURCE_PAGES);
		check(PageDependencyNum <= NANITE_MAX_GROUP_PARTS_MASK);
		PageDependencyStartAndNum = (PageDependencyStart << NANITE_MAX_GROUP_PARTS_BITS) | PageDependencyNum;
	}

	uint32 GetPageIndex() const						{ return PageIndex; }
	uint32 GetNodeIndex() const						{ return HierarchyNodeAndChildIndex >> NANITE_MAX_HIERACHY_CHILDREN_BITS; }
	uint32 GetChildIndex() const					{ return HierarchyNodeAndChildIndex & (NANITE_MAX_HIERACHY_CHILDREN - 1); }
	uint32 GetClusterGroupPartStartIndex() const	{ return ClusterGroupPartStartIndex; }
	uint32 GetPageDependencyStart() const			{ return PageDependencyStartAndNum >> NANITE_MAX_GROUP_PARTS_BITS; }
	uint32 GetPageDependencyNum() const				{ return PageDependencyStartAndNum & NANITE_MAX_GROUP_PARTS_MASK; }

	uint32 PageIndex;
	uint32 HierarchyNodeAndChildIndex;
	uint32 ClusterGroupPartStartIndex;
	uint32 PageDependencyStartAndNum;
};

class FClusterFixup
{
public:
	FClusterFixup() {}

	FClusterFixup( uint32 PageIndex, uint32 ClusterIndex, uint32 PageDependencyStart, uint32 PageDependencyNum )
	{
		check( PageIndex < ( 1 << ( 32 - NANITE_MAX_CLUSTERS_PER_GROUP_BITS ) ) );
		check(ClusterIndex < NANITE_MAX_CLUSTERS_PER_PAGE);
		PageAndClusterIndex = ( PageIndex << NANITE_MAX_CLUSTERS_PER_PAGE_BITS ) | ClusterIndex;

		check(PageDependencyStart < NANITE_MAX_RESOURCE_PAGES);
		check(PageDependencyNum <= NANITE_MAX_GROUP_PARTS_MASK);
		PageDependencyStartAndNum = (PageDependencyStart << NANITE_MAX_GROUP_PARTS_BITS) | PageDependencyNum;
	}
	
	uint32 GetPageIndex() const				{ return PageAndClusterIndex >> NANITE_MAX_CLUSTERS_PER_PAGE_BITS; }
	uint32 GetClusterIndex() const			{ return PageAndClusterIndex & (NANITE_MAX_CLUSTERS_PER_PAGE - 1u); }
	uint32 GetPageDependencyStart() const	{ return PageDependencyStartAndNum >> NANITE_MAX_GROUP_PARTS_BITS; }
	uint32 GetPageDependencyNum() const		{ return PageDependencyStartAndNum & NANITE_MAX_GROUP_PARTS_MASK; }

	uint32 PageAndClusterIndex;
	uint32 PageDependencyStartAndNum;
};

class FFixupChunk	//TODO: rename to something else
{
public:
	struct FHeader
	{
		uint16 Magic = 0;
		uint16 NumClusters = 0;
		uint16 NumHierachyFixups = 0;
		uint16 NumClusterFixups = 0;
	} Header;
	
	uint8 Data[sizeof(FHierarchyFixup) * NANITE_MAX_CLUSTERS_PER_PAGE + sizeof( FClusterFixup ) * NANITE_MAX_CLUSTERS_PER_PAGE];	// One hierarchy fixup per cluster and at most one cluster fixup per cluster.

	FClusterFixup&		GetClusterFixup( uint32 Index ) const { check( Index < Header.NumClusterFixups );  return ( (FClusterFixup*)( Data + Header.NumHierachyFixups * sizeof( FHierarchyFixup ) ) )[ Index ]; }
	FHierarchyFixup&	GetHierarchyFixup( uint32 Index ) const { check( Index < Header.NumHierachyFixups ); return ((FHierarchyFixup*)Data)[ Index ]; }
	uint32				GetSize() const { return sizeof( Header ) + Header.NumHierachyFixups * sizeof( FHierarchyFixup ) + Header.NumClusterFixups * sizeof( FClusterFixup ); }
};

struct FInstanceDraw
{
	uint32 InstanceId;
	uint32 ViewId;
};

struct FResources
{
	// Persistent State
	TArray< uint8 >					RootData;			// Root pages are loaded on resource load, so we always have something to draw.
	FByteBulkData					StreamablePages;	// Remaining pages are streamed on demand.
	TArray< uint16 >				ImposterAtlas;
	TArray< FPackedHierarchyNode >	HierarchyNodes;
	TArray< uint32 >				HierarchyRootOffsets;
	TArray< FPageStreamingState >	PageStreamingStates;
	TArray< uint32 >				PageDependencies;
	uint32							NumRootPages		= 0;
	int32							PositionPrecision	= 0;
	int32							NormalPrecision		= 0;
	int32							TangentPrecision	= 0;
	uint32							NumInputTriangles	= 0;
	uint32							NumInputVertices	= 0;
	uint16							NumInputMeshes		= 0;
	uint16							NumInputTexCoords	= 0;
	uint32							NumClusters			= 0;
	uint32							ResourceFlags		= 0;

	// Runtime State
	uint32	RuntimeResourceID		= 0xFFFFFFFFu;
	uint32	HierarchyOffset			= 0xFFFFFFFFu;
	int32	RootPageIndex			= INDEX_NONE;
	int32	ImposterIndex			= INDEX_NONE;
	uint32	NumHierarchyNodes		= 0;
	uint32	NumResidentClusters		= 0;
	uint32	PersistentHash			= NANITE_INVALID_PERSISTENT_HASH;

#if WITH_EDITOR
	FString							ResourceName;
	FIoHash							DDCKeyHash;
	FIoHash							DDCRawHash;
private:
	TDontCopy<TPimplPtr<UE::DerivedData::FRequestOwner>> DDCRequestOwner;

	enum class EDDCRebuildState : uint8
	{
		Initial,
		Pending,
		Succeeded,
		Failed,
	};

	struct FDDCRebuildState
	{
		std::atomic<EDDCRebuildState> State = EDDCRebuildState::Initial;

		FDDCRebuildState() = default;
		FDDCRebuildState(const FDDCRebuildState&) {}
		FDDCRebuildState& operator=(const FDDCRebuildState&) { check(State == EDDCRebuildState::Initial); return *this; }
	};

	FDDCRebuildState		DDCRebuildState;

	/** Begins an async rebuild of the bulk data from the cache. Must be paired with EndRebuildBulkDataFromCache. */
	ENGINE_API void BeginRebuildBulkDataFromCache(const UObject* Owner);
	/** Ends an async rebuild of the bulk data from the cache. May block if poll has not returned true. */
	ENGINE_API void EndRebuildBulkDataFromCache();
public:
	ENGINE_API void DropBulkData();

	UE_DEPRECATED(5.1, "Use RebuildBulkDataFromCacheAsync instead.")
	ENGINE_API void RebuildBulkDataFromDDC(const UObject* Owner);

	/** Requests (or polls) an async operation that rebuilds the streaming bulk data from the cache.
		If a rebuild is already in progress, the call will just poll the pending operation.
		If true is returned, the operation is complete and it is safe to access the streaming data.
		If false is returned, the operation has not yet completed.
		The operation can fail, which is indicated by the value of bFailed. */
	ENGINE_API bool RebuildBulkDataFromCacheAsync(const UObject* Owner, bool& bFailed);
#endif

	ENGINE_API void InitResources(const UObject* Owner);
	ENGINE_API bool ReleaseResources();

	ENGINE_API void Serialize(FArchive& Ar, UObject* Owner, bool bCooked);
	ENGINE_API bool HasStreamingData() const;

	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;
	bool IsRootPage(uint32 PageIndex) const { return PageIndex < NumRootPages; }
};

class FVertexFactory final : public ::FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE_API(FVertexFactory, ENGINE_API);

public:
	FVertexFactory(ERHIFeatureLevel::Type FeatureLevel) : ::FVertexFactory(FeatureLevel)
	{
	}
	~FVertexFactory()
	{
		ReleaseResource();
	}

	ENGINE_API virtual void InitRHI(FRHICommandListBase& RHICmdList) override final;

	static ENGINE_API bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static ENGINE_API void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static ENGINE_API void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);
};

class FVertexFactoryResource : public FRenderResource
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	FVertexFactory* GetVertexFactory() { return VertexFactory; }
	FNaniteVertexFactory* GetVertexFactory2() { return VertexFactory2; }

private:
	// TODO: Work in progress / experimental (having two factories is temporary).
	// VertexFactory is the legacy VS/PS shading path.
	// VertexFactory2 is the new compute shader path.
	class FVertexFactory* VertexFactory = nullptr;
	class FNaniteVertexFactory* VertexFactory2 = nullptr;
};

} // namespace Nanite

ENGINE_API void ClearNaniteResources(TPimplPtr<Nanite::FResources>& InResources);
ENGINE_API void InitNaniteResources(TPimplPtr<Nanite::FResources>& InResources, bool bRecreate = false);

ENGINE_API uint64 GetNaniteResourcesSize(const TPimplPtr<Nanite::FResources>& InResources);
ENGINE_API void GetNaniteResourcesSizeEx(const TPimplPtr<Nanite::FResources>& InResources, FResourceSizeEx& CumulativeResourceSize);

ENGINE_API uint64 GetNaniteResourcesSize(const Nanite::FResources& InResources);
ENGINE_API void GetNaniteResourcesSizeEx(const Nanite::FResources& InResources, FResourceSizeEx& CumulativeResourceSize);
