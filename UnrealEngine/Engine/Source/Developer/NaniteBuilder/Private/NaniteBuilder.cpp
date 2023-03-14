// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteBuilder.h"
#include "Modules/ModuleManager.h"
#include "Components.h"
#include "StaticMeshResources.h"
#include "Rendering/NaniteResources.h"
#include "Hash/CityHash.h"
#include "GraphPartitioner.h"
#include "Cluster.h"
#include "ClusterDAG.h"
#include "MeshSimplify.h"
#include "DisjointSet.h"
#include "Async/ParallelFor.h"
#include "NaniteEncode.h"
#include "ImposterAtlas.h"
#include "UObject/DevObjectVersion.h"
#include "Compression/OodleDataCompressionUtil.h"

#if WITH_MIKKTSPACE
#include "mikktspace.h"
#endif

#define NANITE_LOG_COMPRESSED_SIZES		0

static TAutoConsoleVariable<bool> CVarBuildImposters(
	TEXT("r.Nanite.Builder.Imposters"),
	false,
	TEXT("Build imposters for small/distant object rendering. For scenes with lots of small or distant objects, imposters can sometimes speed up rendering, but they come at the cost of additional runtime memory and disk footprint overhead."),
	ECVF_ReadOnly
);

namespace Nanite
{

class FBuilderModule : public IBuilderModule
{
public:
	FBuilderModule() {}

	virtual void StartupModule() override
	{
		// Register any modular features here
	}

	virtual void ShutdownModule() override
	{
		// Unregister any modular features here
	}

	virtual const FString& GetVersionString() const;

	virtual bool Build(
		FResources& Resources,
		TArray<FStaticMeshBuildVertex>& Vertices, // TODO: Do not require this vertex type for all users of Nanite
		TArray<uint32>& TriangleIndices,
		TArray<int32>& MaterialIndices,
		TArray<uint32>& MeshTriangleCounts,
		uint32 NumTexCoords,
		const FMeshNaniteSettings& Settings) override;

	virtual bool Build(
		FResources& Resources,
		FVertexMeshData& InputMeshData,
		TArrayView< FVertexMeshData > OutputLODMeshData,
		uint32 NumTexCoords,
		const FMeshNaniteSettings& Settings) override;
};

const FString& FBuilderModule::GetVersionString() const
{
	static FString VersionString;

	if (VersionString.IsEmpty())
	{
		VersionString = FString::Printf(TEXT("%s%s%s%s"), *FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().NANITE_DERIVEDDATA_VER).ToString(EGuidFormats::DigitsWithHyphens),
										NANITE_USE_CONSTRAINED_CLUSTERS ? TEXT("_CONSTRAINED") : TEXT(""),
										NANITE_USE_UNCOMPRESSED_VERTEX_DATA ? TEXT("_UNCOMPRESSED") : TEXT(""),
										CVarBuildImposters.GetValueOnAnyThread() ? TEXT("_IMPOSTERS") : TEXT(""));

#if PLATFORM_CPU_ARM_FAMILY
		// Separate out arm keys as x64 and arm64 clang do not generate the same data for a given
		// input. Add the arm specifically so that a) we avoid rebuilding the current DDC and
		// b) we can remove it once we get arm64 to be consistent.
		VersionString.Append(TEXT("_arm64"));
#endif
	}

	return VersionString;
}

} // namespace Nanite

IMPLEMENT_MODULE( Nanite::FBuilderModule, NaniteBuilder );



namespace Nanite
{

struct FMeshData
{
	TArray< FStaticMeshBuildVertex >&	Verts;
	TArray< uint32 >&					Indexes;
};

static int MikkGetNumFaces( const SMikkTSpaceContext* Context )
{
	FMeshData *UserData = (FMeshData*)Context->m_pUserData;
	return UserData->Indexes.Num() / 3;
}

static int MikkGetNumVertsOfFace( const SMikkTSpaceContext* Context, const int FaceIdx )
{
	return 3;
}

static void MikkGetPosition( const SMikkTSpaceContext* Context, float Position[3], const int FaceIdx, const int VertIdx )
{
	FMeshData *UserData = (FMeshData*)Context->m_pUserData;
	for( int32 i = 0; i < 3; i++ )
		Position[i] = UserData->Verts[ UserData->Indexes[ FaceIdx * 3 + VertIdx ] ].Position[i];
}

static void MikkGetNormal( const SMikkTSpaceContext* Context, float Normal[3], const int FaceIdx, const int VertIdx )
{
	FMeshData *UserData = (FMeshData*)Context->m_pUserData;
	for( int32 i = 0; i < 3; i++ )
		Normal[i] = UserData->Verts[ UserData->Indexes[ FaceIdx * 3 + VertIdx ] ].TangentZ[i];
}

static void MikkSetTSpaceBasic( const SMikkTSpaceContext* Context, const float Tangent[3], const float BitangentSign, const int FaceIdx, const int VertIdx )
{
	FMeshData *UserData = (FMeshData*)Context->m_pUserData;
	for( int32 i = 0; i < 3; i++ )
		UserData->Verts[ UserData->Indexes[ FaceIdx * 3 + VertIdx ] ].TangentX[i] = Tangent[i];

	FVector3f Bitangent = BitangentSign * FVector3f::CrossProduct(
		UserData->Verts[ UserData->Indexes[ FaceIdx * 3 + VertIdx ] ].TangentZ,
		UserData->Verts[ UserData->Indexes[ FaceIdx * 3 + VertIdx ] ].TangentX );

	for( int32 i = 0; i < 3; i++ )
		UserData->Verts[ UserData->Indexes[ FaceIdx * 3 + VertIdx ] ].TangentY[i] = -Bitangent[i];
}

static void MikkGetTexCoord( const SMikkTSpaceContext* Context, float UV[2], const int FaceIdx, const int VertIdx )
{
	FMeshData *UserData = (FMeshData*)Context->m_pUserData;
	for( int32 i = 0; i < 2; i++ )
		UV[i] = UserData->Verts[ UserData->Indexes[ FaceIdx * 3 + VertIdx ] ].UVs[0][i];
}

void CalcTangents(
	TArray< FStaticMeshBuildVertex >& Verts,
	TArray< uint32 >& Indexes )
{
#if WITH_MIKKTSPACE
	FMeshData MeshData = { Verts, Indexes };

	SMikkTSpaceInterface MikkTInterface;
	MikkTInterface.m_getNormal				= MikkGetNormal;
	MikkTInterface.m_getNumFaces			= MikkGetNumFaces;
	MikkTInterface.m_getNumVerticesOfFace	= MikkGetNumVertsOfFace;
	MikkTInterface.m_getPosition			= MikkGetPosition;
	MikkTInterface.m_getTexCoord			= MikkGetTexCoord;
	MikkTInterface.m_setTSpaceBasic			= MikkSetTSpaceBasic;
	MikkTInterface.m_setTSpace				= nullptr;

	SMikkTSpaceContext MikkTContext;
	MikkTContext.m_pInterface				= &MikkTInterface;
	MikkTContext.m_pUserData				= (void*)(&MeshData);
	MikkTContext.m_bIgnoreDegenerates		= true;
	genTangSpaceDefault( &MikkTContext );
#else
	ensureMsgf(false, TEXT("MikkTSpace tangent generation is not supported on this platform."));
#endif //WITH_MIKKTSPACE
}

static float BuildCoarseRepresentation(
	const TArray<FClusterGroup>& Groups,
	const TArray<FCluster>& Clusters,
	TArray<FStaticMeshBuildVertex>& Verts,
	TArray<uint32>& Indexes,
	TArray<FStaticMeshSection, TInlineAllocator<1>>& Sections,
	uint32& NumTexCoords,
	uint32 TargetNumTris,
	float TargetError,
	int32 FallbackLODIndex)
{
	TargetNumTris = FMath::Max( TargetNumTris, 64u );

	FBinaryHeap< float > Heap = FindDAGCut( Groups, Clusters, TargetNumTris, TargetError, 4096, nullptr );

	// Merge
	TArray< const FCluster*, TInlineAllocator<32> > MergeList;
	MergeList.AddUninitialized( Heap.Num() );
	for( uint32 i = 0; i < Heap.Num(); i++ )
	{
		MergeList[i] = &Clusters[ Heap.Peek(i) ];
	}

	// Force a deterministic order
	MergeList.Sort(
		[]( const FCluster& A, const FCluster& B )
		{
			return A.GUID < B.GUID;
		} );

	FCluster CoarseRepresentation( MergeList );
	// FindDAGCut also produces error when TargetError is non-zero but this only happens for LOD0 whose MaxDeviation is always zero.
	// Don't use the old weights for LOD0 since they change the error calculation and hence, change the meaning of TargetError.
	float OutError = CoarseRepresentation.Simplify( TargetNumTris, TargetError, FMath::Min( TargetNumTris, 256u ), FallbackLODIndex > 0 );

	TArray< FStaticMeshSection, TInlineAllocator<1> > OldSections = Sections;

	// Need to update coarse representation UV count to match new data.
	NumTexCoords = CoarseRepresentation.NumTexCoords;

	// Rebuild vertex data
	Verts.Empty(CoarseRepresentation.NumVerts);
	for (uint32 Iter = 0, Num = CoarseRepresentation.NumVerts; Iter < Num; ++Iter)
	{
		FStaticMeshBuildVertex Vertex = {};
		Vertex.Position = CoarseRepresentation.GetPosition(Iter);
		Vertex.TangentX = FVector3f::ZeroVector;
		Vertex.TangentY = FVector3f::ZeroVector;
		Vertex.TangentZ = CoarseRepresentation.GetNormal(Iter);

		const FVector2f* UVs = CoarseRepresentation.GetUVs(Iter);
		for (uint32 UVIndex = 0; UVIndex < NumTexCoords; ++UVIndex)
		{
			Vertex.UVs[UVIndex] = UVs[UVIndex].ContainsNaN() ? FVector2f::ZeroVector : UVs[UVIndex];
		}

		Vertex.Color = CoarseRepresentation.bHasColors ? CoarseRepresentation.GetColor(Iter).ToFColor(false /* sRGB */) : FColor::White;
		
		Verts.Add(Vertex);
	}

	TArray<FMaterialTriangle, TInlineAllocator<128>> CoarseMaterialTris;
	TArray<FMaterialRange, TInlineAllocator<4>> CoarseMaterialRanges;

	// Compute material ranges for coarse representation.
	BuildMaterialRanges(
		CoarseRepresentation.Indexes,
		CoarseRepresentation.MaterialIndexes,
		CoarseMaterialTris,
		CoarseMaterialRanges);
	check(CoarseMaterialRanges.Num() <= OldSections.Num());

	// Rebuild section data.
	Sections.Reset(CoarseMaterialRanges.Num());
	for (const FStaticMeshSection& OldSection : OldSections)
	{
		// Add new sections based on the computed material ranges
		// Enforce the same material order as OldSections
		const FMaterialRange* FoundRange = CoarseMaterialRanges.FindByPredicate([&OldSection](const FMaterialRange& Range) { return Range.MaterialIndex == OldSection.MaterialIndex; });

		// Sections can actually be removed from the coarse mesh if their source data doesn't contain enough triangles
		if (FoundRange)
		{
			// Copy properties from original mesh sections.
			FStaticMeshSection Section(OldSection);

			// Range of vertices and indices used when rendering this section.
			Section.FirstIndex = FoundRange->RangeStart * 3;
			Section.NumTriangles = FoundRange->RangeLength;
			Section.MinVertexIndex = TNumericLimits<uint32>::Max();
			Section.MaxVertexIndex = TNumericLimits<uint32>::Min();

			for (uint32 TriangleIndex = 0; TriangleIndex < (FoundRange->RangeStart + FoundRange->RangeLength); ++TriangleIndex)
			{
				const FMaterialTriangle& Triangle = CoarseMaterialTris[TriangleIndex];

				// Update min vertex index
				Section.MinVertexIndex = FMath::Min(Section.MinVertexIndex, Triangle.Index0);
				Section.MinVertexIndex = FMath::Min(Section.MinVertexIndex, Triangle.Index1);
				Section.MinVertexIndex = FMath::Min(Section.MinVertexIndex, Triangle.Index2);

				// Update max vertex index
				Section.MaxVertexIndex = FMath::Max(Section.MaxVertexIndex, Triangle.Index0);
				Section.MaxVertexIndex = FMath::Max(Section.MaxVertexIndex, Triangle.Index1);
				Section.MaxVertexIndex = FMath::Max(Section.MaxVertexIndex, Triangle.Index2);
			}

			Sections.Add(Section);
		}
	}

	// Rebuild index data.
	Indexes.Reset();
	for (const FMaterialTriangle& Triangle : CoarseMaterialTris)
	{
		Indexes.Add(Triangle.Index0);
		Indexes.Add(Triangle.Index1);
		Indexes.Add(Triangle.Index2);
	}

	CalcTangents(Verts, Indexes);

	return OutError;
}

static void ClusterTriangles(
	const TArray< FStaticMeshBuildVertex >& Verts,
	const TArrayView< const uint32 >& Indexes,
	const TArrayView< const int32 >& MaterialIndexes,
	TArray< FCluster >& Clusters,	// Append
	const FBounds3f& MeshBounds,
	uint32 NumTexCoords,
	bool bHasColors,
	bool bPreserveArea )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::ClusterTriangles);

	uint32 Time0 = FPlatformTime::Cycles();

	LOG_CRC( Verts );
	LOG_CRC( Indexes );

	uint32 NumTriangles = Indexes.Num() / 3;

	FAdjacency Adjacency( Indexes.Num() );
	FEdgeHash EdgeHash( Indexes.Num() );

	auto GetPosition = [ &Verts, &Indexes ]( uint32 EdgeIndex )
	{
		return Verts[ Indexes[ EdgeIndex ] ].Position;
	};

	ParallelFor( TEXT("Nanite.ClusterTriangles.PF"), Indexes.Num(), 4096,
		[&]( int32 EdgeIndex )
		{
			EdgeHash.Add_Concurrent( EdgeIndex, GetPosition );
		} );

	ParallelFor( TEXT("Nanite.ClusterTriangles.PF"), Indexes.Num(), 1024,
		[&]( int32 EdgeIndex )
		{
			int32 AdjIndex = -1;
			int32 AdjCount = 0;
			EdgeHash.ForAllMatching( EdgeIndex, false, GetPosition,
				[&]( int32 EdgeIndex, int32 OtherEdgeIndex )
				{
					AdjIndex = OtherEdgeIndex;
					AdjCount++;
				} );

			if( AdjCount > 1 )
				AdjIndex = -2;

			Adjacency.Direct[ EdgeIndex ] = AdjIndex;
		} );

	FDisjointSet DisjointSet( NumTriangles );

	for( uint32 EdgeIndex = 0, Num = Indexes.Num(); EdgeIndex < Num; EdgeIndex++ )
	{
		if( Adjacency.Direct[ EdgeIndex ] == -2 )
		{
			// EdgeHash is built in parallel, so we need to sort before use to ensure determinism.
			// This path is only executed in the rare event that an edge is shared by more than two triangles,
			// so performance impact should be negligible in practice.
			TArray< TPair< int32, int32 >, TInlineAllocator< 16 > > Edges;
			EdgeHash.ForAllMatching( EdgeIndex, false, GetPosition,
				[&]( int32 EdgeIndex0, int32 EdgeIndex1 )
				{
					Edges.Emplace( EdgeIndex0, EdgeIndex1 );
				} );
			Edges.Sort();	
			
			for( const TPair< int32, int32 >& Edge : Edges )
			{
				Adjacency.Link( Edge.Key, Edge.Value );
			}
		}

		Adjacency.ForAll( EdgeIndex,
			[&]( int32 EdgeIndex0, int32 EdgeIndex1 )
			{
				if( EdgeIndex0 > EdgeIndex1 )
					DisjointSet.UnionSequential( EdgeIndex0 / 3, EdgeIndex1 / 3 );
			} );
	}

	uint32 BoundaryTime = FPlatformTime::Cycles();
	UE_LOG( LogStaticMesh, Log, TEXT("Adjacency [%.2fs], tris: %i, UVs %i%s"), FPlatformTime::ToMilliseconds( BoundaryTime - Time0 ) / 1000.0f, Indexes.Num() / 3, NumTexCoords, bHasColors ? TEXT(", Color") : TEXT("") );

	FGraphPartitioner Partitioner( NumTriangles );

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::PartitionGraph);

		auto GetCenter = [ &Verts, &Indexes ]( uint32 TriIndex )
		{
			FVector3f Center;
			Center  = Verts[ Indexes[ TriIndex * 3 + 0 ] ].Position;
			Center += Verts[ Indexes[ TriIndex * 3 + 1 ] ].Position;
			Center += Verts[ Indexes[ TriIndex * 3 + 2 ] ].Position;
			return Center * (1.0f / 3.0f);
		};
		Partitioner.BuildLocalityLinks( DisjointSet, MeshBounds, MaterialIndexes, GetCenter );

		auto* RESTRICT Graph = Partitioner.NewGraph( NumTriangles * 3 );

		for( uint32 i = 0; i < NumTriangles; i++ )
		{
			Graph->AdjacencyOffset[i] = Graph->Adjacency.Num();

			uint32 TriIndex = Partitioner.Indexes[i];

			for( int k = 0; k < 3; k++ )
			{
				Adjacency.ForAll( 3 * TriIndex + k,
					[ &Partitioner, Graph ]( int32 EdgeIndex, int32 AdjIndex )
					{
						Partitioner.AddAdjacency( Graph, AdjIndex / 3, 4 * 65 );
					} );
			}

			Partitioner.AddLocalityLinks( Graph, TriIndex, 1 );
		}
		Graph->AdjacencyOffset[ NumTriangles ] = Graph->Adjacency.Num();

		bool bSingleThreaded = NumTriangles < 5000;

		Partitioner.PartitionStrict( Graph, FCluster::ClusterSize - 4, FCluster::ClusterSize, !bSingleThreaded );
		check( Partitioner.Ranges.Num() );

		LOG_CRC( Partitioner.Ranges );
	}

	const uint32 OptimalNumClusters = FMath::DivideAndRoundUp< int32 >( Indexes.Num(), FCluster::ClusterSize * 3 );

	uint32 ClusterTime = FPlatformTime::Cycles();
	UE_LOG( LogStaticMesh, Log, TEXT("Clustering [%.2fs]. Ratio: %f"), FPlatformTime::ToMilliseconds( ClusterTime - BoundaryTime ) / 1000.0f, (float)Partitioner.Ranges.Num() / OptimalNumClusters );

	const uint32 BaseCluster = Clusters.Num();
	Clusters.AddDefaulted( Partitioner.Ranges.Num() );

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::BuildClusters);
		ParallelFor( TEXT("Nanite.BuildClusters.PF"), Partitioner.Ranges.Num(), 1024,
			[&]( int32 Index )
			{
				auto& Range = Partitioner.Ranges[ Index ];

				Clusters[ BaseCluster + Index ] = FCluster(
					Verts,
					Indexes,
					MaterialIndexes,
					NumTexCoords, bHasColors, bPreserveArea,
					Range.Begin, Range.End, Partitioner, Adjacency );

				// Negative notes it's a leaf
				Clusters[ BaseCluster + Index ].EdgeLength *= -1.0f;
			});
	}

	uint32 LeavesTime = FPlatformTime::Cycles();
	UE_LOG( LogStaticMesh, Log, TEXT("Leaves [%.2fs]"), FPlatformTime::ToMilliseconds( LeavesTime - ClusterTime ) / 1000.0f );
}

#if NANITE_LOG_COMPRESSED_SIZES
static void CalculateCompressedNaniteDiskSize(FResources& Resources, int32& OutUncompressedSize, int32& OutCompressedSize)
{
	TArray<uint8> Data;
	FMemoryWriter Ar(Data, true);
	Resources.Serialize(Ar, nullptr, true);
	OutUncompressedSize = Data.Num();

	TArray<uint8> CompressedData;
	FOodleCompressedArray::CompressTArray(CompressedData, Data, FOodleDataCompression::ECompressor::Mermaid, FOodleDataCompression::ECompressionLevel::Optimal2);
	OutCompressedSize = CompressedData.Num();
}
#endif

static bool BuildNaniteData(
	FResources& Resources,
	IBuilderModule::FVertexMeshData& InputMeshData,
	TArray< int32 >& MaterialIndexes,
	TArray< uint32 >& MeshTriangleCounts,
	TArrayView< IBuilderModule::FVertexMeshData >& OutputLODMeshData,
	uint32 NumTexCoords,
	const FMeshNaniteSettings& Settings
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::BuildData);

	if (NumTexCoords > NANITE_MAX_UVS)
	{
		NumTexCoords = NANITE_MAX_UVS;
	}

	FBounds3f	VertexBounds;
	uint32 Channel = 255;
	for( auto& Vert : InputMeshData.Vertices )
	{
		VertexBounds += Vert.Position;

		Channel &= Vert.Color.R;
		Channel &= Vert.Color.G;
		Channel &= Vert.Color.B;
		Channel &= Vert.Color.A;
	}

	Resources.NumInputTriangles	= InputMeshData.TriangleIndices.Num() / 3;
	Resources.NumInputVertices	= InputMeshData.Vertices.Num();
	Resources.NumInputMeshes	= MeshTriangleCounts.Num();
	Resources.NumInputTexCoords = NumTexCoords;

	// Don't trust any input. We only have color if it isn't all white.
	const bool bHasVertexColor = Channel != 255;
	const bool bHasImposter = CVarBuildImposters.GetValueOnAnyThread() && (Resources.NumInputMeshes == 1);

	Resources.ResourceFlags = 0x0;

	if (bHasVertexColor)
	{
		Resources.ResourceFlags |= NANITE_RESOURCE_FLAG_HAS_VERTEX_COLOR;
	}

	if (bHasImposter)
	{
		Resources.ResourceFlags |= NANITE_RESOURCE_FLAG_HAS_IMPOSTER;
	}

	TArray< uint32 > ClusterCountPerMesh;
	TArray< FCluster > Clusters;
	{
		uint32 BaseTriangle = 0;
		for (uint32 NumTriangles : MeshTriangleCounts)
		{
			uint32 NumClustersBefore = Clusters.Num();
			if (NumTriangles)
			{
				ClusterTriangles(
					InputMeshData.Vertices,
					TArrayView< const uint32 >( &InputMeshData.TriangleIndices[BaseTriangle * 3], NumTriangles * 3 ),
					TArrayView< const int32 >( &MaterialIndexes[BaseTriangle], NumTriangles ),
					Clusters, VertexBounds, NumTexCoords, bHasVertexColor, Settings.bPreserveArea );
			}
			ClusterCountPerMesh.Add(Clusters.Num() - NumClustersBefore);
			BaseTriangle += NumTriangles;
		}
	}

	float SurfaceArea = 0.0f;
	for( FCluster& Cluster : Clusters )
		SurfaceArea += Cluster.SurfaceArea;

	int32 FallbackTargetNumTris = Resources.NumInputTriangles * Settings.FallbackPercentTriangles;
	float FallbackTargetError = Settings.FallbackRelativeError * 0.01f * FMath::Sqrt( FMath::Min( 2.0f * SurfaceArea, VertexBounds.GetSurfaceArea() ) );

	bool bFallbackIsReduced = Settings.FallbackPercentTriangles < 1.0f || FallbackTargetError > 0.0f;
	
	// If we're going to replace the original vertex buffer with a coarse representation, get rid of the old copies
	// now that we copied it into the cluster representation. We do it before the longer DAG reduce phase to shorten peak memory duration.
	// This is especially important when building multiple huge Nanite meshes in parallel.
	if( bFallbackIsReduced )
	{
		InputMeshData.Vertices.Empty();
		InputMeshData.TriangleIndices.Empty();
	}
	MaterialIndexes.Empty();

	uint32 Time0 = FPlatformTime::Cycles();

	FBounds3f MeshBounds;	
	TArray<FClusterGroup> Groups;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::DAG.Reduce);
		
		uint32 ClusterStart = 0;
		for (uint32 MeshIndex = 0; MeshIndex < Resources.NumInputMeshes; MeshIndex++)
		{
			uint32 NumClusters = ClusterCountPerMesh[MeshIndex];
			BuildDAG( Groups, Clusters, ClusterStart, NumClusters, MeshIndex, MeshBounds );
			ClusterStart += NumClusters;
		}
	}
	
	if( Settings.KeepPercentTriangles < 1.0f || Settings.TrimRelativeError > 0.0f )
	{
		int32 TargetNumTris = Resources.NumInputTriangles * Settings.KeepPercentTriangles;
		float TargetError = Settings.TrimRelativeError * 0.01f * FMath::Sqrt( FMath::Min( 2.0f * SurfaceArea, VertexBounds.GetSurfaceArea() ) );

		TBitArray<> SelectedGroupsMask;
		FBinaryHeap< float > Heap = FindDAGCut( Groups, Clusters, TargetNumTris, TargetError, 0, &SelectedGroupsMask );

		for( int32 GroupIndex = 0; GroupIndex < SelectedGroupsMask.Num(); GroupIndex++ )
		{
			Groups[ GroupIndex ].bTrimmed = !SelectedGroupsMask[ GroupIndex ];
		}
	
		uint32 NumVerts = 0;
		uint32 NumTris = 0;
		for( uint32 i = 0; i < Heap.Num(); i++ )
		{
			FCluster& Cluster = Clusters[ Heap.Peek(i) ];

			Cluster.GeneratingGroupIndex = MAX_uint32;
			Cluster.EdgeLength = -FMath::Abs( Cluster.EdgeLength );
			NumVerts += Cluster.NumVerts;
			NumTris  += Cluster.NumTris;
		}

		Resources.NumInputVertices	= FMath::Min( NumVerts, Resources.NumInputVertices );
		Resources.NumInputTriangles	= NumTris;

		UE_LOG( LogStaticMesh, Log, TEXT("Trimmed to %u tris"), NumTris );
	}

	uint32 ReduceTime = FPlatformTime::Cycles();
	UE_LOG(LogStaticMesh, Log, TEXT("Reduce [%.2fs]"), FPlatformTime::ToMilliseconds(ReduceTime - Time0) / 1000.0f);

	for (int32 FallbackLODIndex = 0; FallbackLODIndex < OutputLODMeshData.Num(); ++FallbackLODIndex)
	{
		const uint32 FallbackStartTime = FPlatformTime::Cycles();

		auto& FallbackLODMeshData = OutputLODMeshData[FallbackLODIndex];
		
		// Copy the section data which will then be patched up after the simplification
		FallbackLODMeshData.Sections = InputMeshData.Sections;
		
		// % of first proxy not % of original
		if( FallbackLODIndex > 0 )
		{
			FallbackTargetNumTris = OutputLODMeshData[0].TriangleIndices.Num() / 3;
			FallbackTargetNumTris *= FallbackLODMeshData.PercentTriangles;
			FallbackTargetError = 0.0f;
		}

		if( !bFallbackIsReduced && FallbackLODIndex == 0 )
		{
			Swap( FallbackLODMeshData.Vertices,			InputMeshData.Vertices );
			Swap( FallbackLODMeshData.TriangleIndices,	InputMeshData.TriangleIndices );
			FallbackLODMeshData.MaxDeviation = 0.f;
		}
		else
		{
			TArray<FStaticMeshSection, TInlineAllocator<1>> FallbackSections = InputMeshData.Sections;
			const float ReductionError = BuildCoarseRepresentation(Groups, Clusters, FallbackLODMeshData.Vertices, FallbackLODMeshData.TriangleIndices, FallbackSections, NumTexCoords, FallbackTargetNumTris, FallbackTargetError, FallbackLODIndex);

			FallbackLODMeshData.MaxDeviation = FallbackLODIndex == 0 ? 0.f : ReductionError / 8.f;

			// Fixup mesh section info with new coarse mesh ranges, while respecting original ordering and keeping materials
			// that do not end up with any assigned triangles (due to decimation process).

			for (FStaticMeshSection& Section : FallbackLODMeshData.Sections)
			{
				// For each section info, try to find a matching entry in the coarse version.
				const FStaticMeshSection* FallbackSection = FallbackSections.FindByPredicate(
					[&Section](const FStaticMeshSection& CoarseSectionIter)
					{
						return CoarseSectionIter.MaterialIndex == Section.MaterialIndex;
					});

				if (FallbackSection != nullptr)
				{
					// Matching entry found
					Section.FirstIndex		= FallbackSection->FirstIndex;
					Section.NumTriangles	= FallbackSection->NumTriangles;
					Section.MinVertexIndex	= FallbackSection->MinVertexIndex;
					Section.MaxVertexIndex	= FallbackSection->MaxVertexIndex;
				}
				else
				{
					// Section removed due to decimation, set placeholder entry
					Section.FirstIndex		= 0;
					Section.NumTriangles	= 0;
					Section.MinVertexIndex	= 0;
					Section.MaxVertexIndex	= 0;
				}
			}
		}

		const uint32 FallbackEndTime = FPlatformTime::Cycles();
		UE_LOG(LogStaticMesh, Log, TEXT("Fallback %d/%d [%.2fs], num tris: %d"), FallbackLODIndex, OutputLODMeshData.Num(), FPlatformTime::ToMilliseconds(FallbackEndTime - FallbackStartTime) / 1000.0f, FallbackLODMeshData.TriangleIndices.Num() / 3);
	}

	uint32 EncodeTime0 = FPlatformTime::Cycles();

	Encode( Resources, Settings, Clusters, Groups, MeshBounds, Resources.NumInputMeshes, NumTexCoords, bHasVertexColor);

	uint32 EncodeTime1 = FPlatformTime::Cycles();
	UE_LOG( LogStaticMesh, Log, TEXT("Encode [%.2fs]"), FPlatformTime::ToMilliseconds( EncodeTime1 - EncodeTime0 ) / 1000.0f );

	if (bHasImposter)
	{
		uint32 ImposterStartTime = FPlatformTime::Cycles();
		auto& RootChildren = Groups.Last().Children;
	
		FImposterAtlas ImposterAtlas( Resources.ImposterAtlas, MeshBounds );

		ParallelFor( TEXT("Nanite.BuildData.PF"), FMath::Square(FImposterAtlas::AtlasSize), 1,
			[&]( int32 TileIndex )
			{
				FIntPoint TilePos(
					TileIndex % FImposterAtlas::AtlasSize,
					TileIndex / FImposterAtlas::AtlasSize);

				for( int32 ClusterIndex = 0; ClusterIndex < RootChildren.Num(); ClusterIndex++ )
				{
					ImposterAtlas.Rasterize( TilePos, Clusters[ RootChildren[ ClusterIndex ] ], ClusterIndex) ;
				}
			} );

		UE_LOG(LogStaticMesh, Log, TEXT("Imposter [%.2fs]"), FPlatformTime::ToMilliseconds(FPlatformTime::Cycles() - ImposterStartTime ) / 1000.0f);
	}

#if NANITE_LOG_COMPRESSED_SIZES
	int32 UncompressedSize, CompressedSize;
	CalculateCompressedNaniteDiskSize(Resources, UncompressedSize, CompressedSize);
	UE_LOG(LogStaticMesh, Log, TEXT("Compressed size: %.2fMB -> %.2fMB"), UncompressedSize / 1048576.0f, CompressedSize / 1048576.0f);

	{
		static FCriticalSection CriticalSection;
		FScopeLock Lock(&CriticalSection);
		static uint32 TotalMeshes = 0;
		static uint64 TotalMeshUncompressedSize = 0;
		static uint64 TotalMeshCompressedSize = 0;

		TotalMeshes++;
		TotalMeshUncompressedSize += UncompressedSize;
		TotalMeshCompressedSize += CompressedSize;
		UE_LOG(LogStaticMesh, Log, TEXT("Total: %d Meshes, Uncompressed: %.2fMB, Compressed: %.2fMB"), TotalMeshes, TotalMeshUncompressedSize / 1048576.0f, TotalMeshCompressedSize / 1048576.0f);
	}
#endif

	uint32 Time1 = FPlatformTime::Cycles();

	UE_LOG( LogStaticMesh, Log, TEXT("Nanite build [%.2fs]\n"), FPlatformTime::ToMilliseconds( Time1 - Time0 ) / 1000.0f );

	return true;
}

bool FBuilderModule::Build(
	FResources& Resources,
	TArray<FStaticMeshBuildVertex>& Vertices, // TODO: Do not require this vertex type for all users of Nanite
	TArray<uint32>& TriangleIndices,
	TArray<int32>&  MaterialIndices,
	TArray<uint32>& MeshTriangleCounts,
	uint32 NumTexCoords,
	const FMeshNaniteSettings& Settings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build);

	check(Settings.FallbackPercentTriangles == 1.0f); // No coarse representation used by this path

	FVertexMeshData InputMeshData;
	InputMeshData.Vertices = Vertices;
	InputMeshData.TriangleIndices = TriangleIndices;
	// Section are left empty because they are not touched anyway (not building a coarse representation)
	
	TArrayView< FVertexMeshData > EmptyOutputLODMeshData;
	
	return BuildNaniteData(
		Resources,
		InputMeshData,
		MaterialIndices,
		MeshTriangleCounts,
		EmptyOutputLODMeshData,
		NumTexCoords,
		Settings
	);
}

bool FBuilderModule::Build(
	FResources& Resources,
	FVertexMeshData& InputMeshData,
	TArrayView< FVertexMeshData > OutputLODMeshData,
	uint32 NumTexCoords,
	const FMeshNaniteSettings& Settings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build);

	// TODO: Properly error out if # of unique materials is > 64 (error message to editor log)
	check(InputMeshData.Sections.Num() > 0 && InputMeshData.Sections.Num() <= 64);

	// Build associated array of triangle index and material index.
	TArray<int32> MaterialIndices;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::BuildSections);
		MaterialIndices.Reserve(InputMeshData.TriangleIndices.Num() / 3);
		for (int32 SectionIndex = 0; SectionIndex < InputMeshData.Sections.Num(); SectionIndex++)
		{
			FStaticMeshSection& Section = InputMeshData.Sections[SectionIndex];

			// TODO: Safe to enforce valid materials always?
			check(Section.MaterialIndex != INDEX_NONE);
			for (uint32 i = 0; i < Section.NumTriangles; ++i)
			{
				MaterialIndices.Add(Section.MaterialIndex);
			}
		}
	}

	TArray<uint32> MeshTriangleCounts;
	MeshTriangleCounts.Add(InputMeshData.TriangleIndices.Num() / 3);

	// Make sure there is 1 material index per triangle.
	check(MaterialIndices.Num() * 3 == InputMeshData.TriangleIndices.Num());

	return BuildNaniteData(
		Resources,
		InputMeshData,
		MaterialIndices,
		MeshTriangleCounts,
		OutputLODMeshData,
		NumTexCoords,
		Settings
	);
}

} // namespace Nanite
