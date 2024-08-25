// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClusterDAG.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "GraphPartitioner.h"
#include "MeshSimplify.h"

namespace Nanite
{

static const uint32 MinGroupSize = 8;
static const uint32 MaxGroupSize = 32;

static void DAGReduce( TArray< FClusterGroup >& Groups, TArray< FCluster >& Clusters, TAtomic< uint32 >& NumClusters, TArrayView< uint32 > Children, int32 GroupIndex, uint32 MeshIndex );

void BuildDAG( TArray< FClusterGroup >& Groups, TArray< FCluster >& Clusters, uint32 ClusterRangeStart, uint32 ClusterRangeNum, uint32 MeshIndex, FBounds3f& MeshBounds )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Nanite.BuildDAG);

	if( ClusterRangeNum == 0 )
	{
		return;
	}

	uint32 LevelOffset	= ClusterRangeStart;
	
	TAtomic< uint32 > NumClusters( Clusters.Num() );

	bool bFirstLevel = true;

	while( true )
	{
		TArrayView< FCluster > LevelClusters( &Clusters[LevelOffset], bFirstLevel ? ClusterRangeNum : (Clusters.Num() - LevelOffset) );
		bFirstLevel = false;

		uint32 NumExternalEdges = 0;

		float MinError = +MAX_flt;
		float MaxError = -MAX_flt;
		float AvgError = 0.0f;

		for( FCluster& Cluster : LevelClusters )
		{
			NumExternalEdges	+= Cluster.NumExternalEdges;
			MeshBounds			+= Cluster.Bounds;

			MinError = FMath::Min( MinError, Cluster.LODError );
			MaxError = FMath::Max( MaxError, Cluster.LODError );
			AvgError += Cluster.LODError;
		}
		AvgError /= (float)LevelClusters.Num();

		UE_LOG( LogStaticMesh, Verbose, TEXT("Num clusters %i. Error %.4f, %.4f, %.4f"), LevelClusters.Num(), MinError, AvgError, MaxError );

		if( LevelClusters.Num() < 2 )
			break;

		if( LevelClusters.Num() <= MaxGroupSize )
		{
			TArray< uint32, TInlineAllocator< MaxGroupSize > > Children;

			uint32 MaxParents = 0;
			for( FCluster& Cluster : LevelClusters )
			{
				MaxParents += FMath::DivideAndRoundUp< uint32 >( Cluster.Indexes.Num(), FCluster::ClusterSize * 6 );
				Children.Add( LevelOffset++ );
			}

			LevelOffset = Clusters.Num();
			Clusters.AddDefaulted( MaxParents );
			Groups.AddDefaulted( 1 );

			DAGReduce( Groups, Clusters, NumClusters, Children, Groups.Num() - 1, MeshIndex );

			// Correct num to atomic count
			Clusters.SetNum( NumClusters, EAllowShrinking::No );

			continue;
		}
		
		struct FExternalEdge
		{
			uint32	ClusterIndex;
			int32	EdgeIndex;
		};
		TArray< FExternalEdge >	ExternalEdges;
		FHashTable				ExternalEdgeHash;
		TAtomic< uint32 >		ExternalEdgeOffset(0);

		// We have a total count of NumExternalEdges so we can allocate a hash table without growing.
		ExternalEdges.AddUninitialized( NumExternalEdges );
		ExternalEdgeHash.Clear( 1 << FMath::FloorLog2( NumExternalEdges ), NumExternalEdges );

		// Add edges to hash table
		ParallelFor( TEXT("Nanite.BuildDAG.PF"), LevelClusters.Num(), 32,
			[&]( uint32 ClusterIndex )
			{
				FCluster& Cluster = LevelClusters[ ClusterIndex ];

				for( int32 EdgeIndex = 0; EdgeIndex < Cluster.ExternalEdges.Num(); EdgeIndex++ )
				{
					if( Cluster.ExternalEdges[ EdgeIndex ] )
					{
						uint32 VertIndex0 = Cluster.Indexes[ EdgeIndex ];
						uint32 VertIndex1 = Cluster.Indexes[ Cycle3( EdgeIndex ) ];
	
						const FVector3f& Position0 = Cluster.GetPosition( VertIndex0 );
						const FVector3f& Position1 = Cluster.GetPosition( VertIndex1 );

						uint32 Hash0 = HashPosition( Position0 );
						uint32 Hash1 = HashPosition( Position1 );
						uint32 Hash = Murmur32( { Hash0, Hash1 } );

						uint32 ExternalEdgeIndex = ExternalEdgeOffset++;
						ExternalEdges[ ExternalEdgeIndex ] = { ClusterIndex, EdgeIndex };
						ExternalEdgeHash.Add_Concurrent( Hash, ExternalEdgeIndex );
					}
				}
			});

		check( ExternalEdgeOffset == ExternalEdges.Num() );

		TAtomic< uint32 > NumAdjacency(0);

		// Find matching edge in other clusters
		ParallelFor( TEXT("Nanite.BuildDAG.PF"), LevelClusters.Num(), 32,
			[&]( uint32 ClusterIndex )
			{
				FCluster& Cluster = LevelClusters[ ClusterIndex ];

				for( int32 EdgeIndex = 0; EdgeIndex < Cluster.ExternalEdges.Num(); EdgeIndex++ )
				{
					if( Cluster.ExternalEdges[ EdgeIndex ] )
					{
						uint32 VertIndex0 = Cluster.Indexes[ EdgeIndex ];
						uint32 VertIndex1 = Cluster.Indexes[ Cycle3( EdgeIndex ) ];
	
						const FVector3f& Position0 = Cluster.GetPosition( VertIndex0 );
						const FVector3f& Position1 = Cluster.GetPosition( VertIndex1 );

						uint32 Hash0 = HashPosition( Position0 );
						uint32 Hash1 = HashPosition( Position1 );
						uint32 Hash = Murmur32( { Hash1, Hash0 } );

						for( uint32 ExternalEdgeIndex = ExternalEdgeHash.First( Hash ); ExternalEdgeHash.IsValid( ExternalEdgeIndex ); ExternalEdgeIndex = ExternalEdgeHash.Next( ExternalEdgeIndex ) )
						{
							FExternalEdge ExternalEdge = ExternalEdges[ ExternalEdgeIndex ];

							FCluster& OtherCluster = LevelClusters[ ExternalEdge.ClusterIndex ];

							if( OtherCluster.ExternalEdges[ ExternalEdge.EdgeIndex ] )
							{
								uint32 OtherVertIndex0 = OtherCluster.Indexes[ ExternalEdge.EdgeIndex ];
								uint32 OtherVertIndex1 = OtherCluster.Indexes[ Cycle3( ExternalEdge.EdgeIndex ) ];
			
								if( Position0 == OtherCluster.GetPosition( OtherVertIndex1 ) &&
									Position1 == OtherCluster.GetPosition( OtherVertIndex0 ) )
								{
									if( ClusterIndex != ExternalEdge.ClusterIndex )
									{
										// Increase it's count
										Cluster.AdjacentClusters.FindOrAdd( ExternalEdge.ClusterIndex, 0 )++;

										// Can't break or a triple edge might be non-deterministically connected.
										// Need to find all matching, not just first.
									}
								}
							}
						}
					}
				}
				NumAdjacency += Cluster.AdjacentClusters.Num();

				// Force deterministic order of adjacency.
				Cluster.AdjacentClusters.KeySort(
					[ &LevelClusters ]( uint32 A, uint32 B )
					{
						return LevelClusters[A].GUID < LevelClusters[B].GUID;
					} );
			});

		FDisjointSet DisjointSet( LevelClusters.Num() );

		for( uint32 ClusterIndex = 0; ClusterIndex < (uint32)LevelClusters.Num(); ClusterIndex++ )
		{
			for( auto& Pair : LevelClusters[ ClusterIndex ].AdjacentClusters )
			{
				uint32 OtherClusterIndex = Pair.Key;

				uint32 Count = LevelClusters[ OtherClusterIndex ].AdjacentClusters.FindChecked( ClusterIndex );
				check( Count == Pair.Value );

				if( ClusterIndex > OtherClusterIndex )
				{
					DisjointSet.UnionSequential( ClusterIndex, OtherClusterIndex );
				}
			}
		}

		FGraphPartitioner Partitioner( LevelClusters.Num() );

		// Sort to force deterministic order
		Partitioner.Indexes.Sort(
			[&LevelClusters](uint32 A, uint32 B)
			{
				return LevelClusters[A].GUID < LevelClusters[B].GUID;
			} );

		auto GetCenter = [&]( uint32 Index )
		{
			FBounds3f& Bounds = LevelClusters[ Index ].Bounds;
			return 0.5f * ( Bounds.Min + Bounds.Max );
		};
		Partitioner.BuildLocalityLinks( DisjointSet, MeshBounds, TArrayView< const int32 >(), GetCenter );

		auto* RESTRICT Graph = Partitioner.NewGraph( NumAdjacency );

		for( int32 i = 0; i < LevelClusters.Num(); i++ )
		{
			Graph->AdjacencyOffset[i] = Graph->Adjacency.Num();

			uint32 ClusterIndex = Partitioner.Indexes[i];

			for( auto& Pair : LevelClusters[ ClusterIndex ].AdjacentClusters )
			{
				uint32 OtherClusterIndex = Pair.Key;
				uint32 NumSharedEdges = Pair.Value;

				const auto& Cluster0 = Clusters[ LevelOffset + ClusterIndex ];
				const auto& Cluster1 = Clusters[ LevelOffset + OtherClusterIndex ];

				bool bSiblings = Cluster0.GroupIndex != MAX_uint32 && Cluster0.GroupIndex == Cluster1.GroupIndex;

				Partitioner.AddAdjacency( Graph, OtherClusterIndex, NumSharedEdges * ( bSiblings ? 1 : 16 ) + 4 );
			}

			Partitioner.AddLocalityLinks( Graph, ClusterIndex, 1 );
		}
		Graph->AdjacencyOffset[ Graph->Num ] = Graph->Adjacency.Num();

		LOG_CRC( Graph->Adjacency );
		LOG_CRC( Graph->AdjacencyCost );
		LOG_CRC( Graph->AdjacencyOffset );
		
		bool bSingleThreaded = LevelClusters.Num() <= 32;

		Partitioner.PartitionStrict( Graph, MinGroupSize, MaxGroupSize, !bSingleThreaded );

		LOG_CRC( Partitioner.Ranges );

		uint32 MaxParents = 0;
		for( auto& Range : Partitioner.Ranges )
		{
			uint32 NumParentIndexes = 0;
			for( uint32 i = Range.Begin; i < Range.End; i++ )
			{
				// Global indexing is needed in Reduce()
				Partitioner.Indexes[i] += LevelOffset;
				NumParentIndexes += Clusters[ Partitioner.Indexes[i] ].Indexes.Num();
			}
			MaxParents += FMath::DivideAndRoundUp( NumParentIndexes, FCluster::ClusterSize * 6 );
		}

		LevelOffset = Clusters.Num();

		const uint32 ParentsStartOffset = Clusters.Num();
		Clusters.AddDefaulted( MaxParents );
		Groups.AddDefaulted( Partitioner.Ranges.Num() );

		ParallelFor( TEXT("Nanite.BuildDAG.PF"), Partitioner.Ranges.Num(), 1,
			[&]( int32 PartitionIndex )
			{
				auto& Range = Partitioner.Ranges[ PartitionIndex ];

				TArrayView< uint32 > Children( &Partitioner.Indexes[ Range.Begin ], Range.End - Range.Begin );

				// Force a deterministic order
				Children.Sort(
					[&]( uint32 A, uint32 B )
					{
						return Clusters[A].GUID < Clusters[B].GUID;
					} );

				uint32 ClusterGroupIndex = PartitionIndex + Groups.Num() - Partitioner.Ranges.Num();

				DAGReduce( Groups, Clusters, NumClusters, Children, ClusterGroupIndex, MeshIndex );
			} );

		// Correct num to atomic count
		Clusters.SetNum( NumClusters, EAllowShrinking::No );

		// Force a deterministic order of the generated parent clusters
		{
			// TODO: Optimize me.
			// Just sorting the array directly seems like the safest option at this stage (right before UE5 final build).
			// On AOD_Shield this seems to be on the order of 0.01s in practice.
			// As the Clusters array is already conservatively allocated, it seems storing the parent clusters in their designated
			// conservative ranges and then doing a compaction pass at the end would be a more efficient solution that doesn't involve sorting.
			
			//uint32 StartTime = FPlatformTime::Cycles();
			TArrayView< FCluster > Parents( Clusters.GetData() + ParentsStartOffset, NumClusters - ParentsStartOffset );
			Parents.Sort(
				[&]( const FCluster& A, const FCluster& B )
				{
					return A.GUID < B.GUID;
				} );
			//UE_LOG(LogStaticMesh, Log, TEXT("SortTime Adjacency [%.2fs]"), FPlatformTime::ToMilliseconds(FPlatformTime::Cycles() - StartTime) / 1000.0f);
		}
		
	}
	
	// Max out root node
	uint32 RootIndex = LevelOffset;
	FClusterGroup RootClusterGroup;
	RootClusterGroup.Children.Add( RootIndex );
	RootClusterGroup.Bounds				= Clusters[ RootIndex ].SphereBounds;
	RootClusterGroup.LODBounds			= FSphere3f( 0 );
	RootClusterGroup.MaxParentLODError	= 1e10f;
	RootClusterGroup.MinLODError		= -1.0f;
	RootClusterGroup.MipLevel			= Clusters[ RootIndex ].MipLevel + 1;
	RootClusterGroup.MeshIndex			= MeshIndex;
	RootClusterGroup.bTrimmed			= false;
	Clusters[ RootIndex ].GroupIndex = Groups.Num();
	Groups.Add( RootClusterGroup );
}

static void DAGReduce( TArray< FClusterGroup >& Groups, TArray< FCluster >& Clusters, TAtomic< uint32 >& NumClusters, TArrayView< uint32 > Children, int32 GroupIndex, uint32 MeshIndex )
{
	check( GroupIndex >= 0 );

	// Merge
	TArray< const FCluster*, TInlineAllocator<32> > MergeList;
	for( int32 Child : Children )
	{
		MergeList.Add( &Clusters[ Child ] );
	}
	
	// Force a deterministic order
	MergeList.Sort(
		[]( const FCluster& A, const FCluster& B )
		{
			return A.GUID < B.GUID;
		} );

	FCluster Merged( MergeList );

	int32 NumParents = FMath::DivideAndRoundUp< int32 >( Merged.Indexes.Num(), FCluster::ClusterSize * 6 );
	int32 ParentStart = 0;
	int32 ParentEnd = 0;

	float ParentMaxLODError = 0.0f;

	for( int32 TargetClusterSize = FCluster::ClusterSize - 2; TargetClusterSize > FCluster::ClusterSize / 2; TargetClusterSize -= 2 )
	{
		int32 TargetNumTris = NumParents * TargetClusterSize;

		// Simplify
		ParentMaxLODError = Merged.Simplify( TargetNumTris );

		// Split
		if( NumParents == 1 )
		{
			ParentEnd = ( NumClusters += NumParents );
			ParentStart = ParentEnd - NumParents;

			Clusters[ ParentStart ] = Merged;
			Clusters[ ParentStart ].Bound();
			break;
		}
		else
		{
			FAdjacency Adjacency = Merged.BuildAdjacency();

			FGraphPartitioner Partitioner( Merged.Indexes.Num() / 3 );
			Merged.Split( Partitioner, Adjacency );

			if( Partitioner.Ranges.Num() <= NumParents )
			{
				NumParents = Partitioner.Ranges.Num();
				ParentEnd = ( NumClusters += NumParents );
				ParentStart = ParentEnd - NumParents;

				int32 Parent = ParentStart;
				for( auto& Range : Partitioner.Ranges )
				{
					Clusters[ Parent ] = FCluster( Merged, Range.Begin, Range.End, Partitioner, Adjacency );
					Parent++;
				}

				break;
			}
		}

		// Start over from scratch. Continuing from simplified cluster screws up ExternalEdges and LODError.
		Merged = FCluster( MergeList );
	}

	TArray< FSphere3f, TInlineAllocator<32> > Children_LODBounds;
	TArray< FSphere3f, TInlineAllocator<32> > Children_SphereBounds;
					
	// Force monotonic nesting.
	float ChildMinLODError = MAX_flt;
	for( int32 Child : Children )
	{
		bool bLeaf = Clusters[ Child ].EdgeLength < 0.0f;
		float LODError = Clusters[ Child ].LODError;

		Children_LODBounds.Add( Clusters[ Child ].LODBounds );
		Children_SphereBounds.Add( Clusters[ Child ].SphereBounds );
		ChildMinLODError = FMath::Min( ChildMinLODError, bLeaf ? -1.0f : LODError );
		ParentMaxLODError = FMath::Max( ParentMaxLODError, LODError );

		Clusters[ Child ].GroupIndex = GroupIndex;
		Groups[ GroupIndex ].Children.Add( Child );
		check( Groups[ GroupIndex ].Children.Num() <= NANITE_MAX_CLUSTERS_PER_GROUP_TARGET );
	}
	
	FSphere3f ParentLODBounds( Children_LODBounds.GetData(), Children_LODBounds.Num() );
	FSphere3f ParentBounds( Children_SphereBounds.GetData(), Children_SphereBounds.Num() );

	// Force parents to have same LOD data. They are all dependent.
	for( int32 Parent = ParentStart; Parent < ParentEnd; Parent++ )
	{
		Clusters[ Parent ].LODBounds			= ParentLODBounds;
		Clusters[ Parent ].LODError				= ParentMaxLODError;
		Clusters[ Parent ].GeneratingGroupIndex = GroupIndex;
	}

	Groups[ GroupIndex ].Bounds				= ParentBounds;
	Groups[ GroupIndex ].LODBounds			= ParentLODBounds;
	Groups[ GroupIndex ].MinLODError		= ChildMinLODError;
	Groups[ GroupIndex ].MaxParentLODError	= ParentMaxLODError;
	Groups[ GroupIndex ].MipLevel			= Merged.MipLevel - 1;
	Groups[ GroupIndex ].MeshIndex			= MeshIndex;
	Groups[ GroupIndex ].bTrimmed			= false;
}

FBinaryHeap< float > FindDAGCut(
	const TArray< FClusterGroup >& Groups,
	const TArray< FCluster >& Clusters,
	uint32 TargetNumTris,
	float  TargetError,
	uint32 TargetOvershoot,
	TBitArray<>* SelectedGroupsMask )
{
	const FClusterGroup&	RootGroup = Groups.Last();
	const FCluster&			RootCluster = Clusters[ RootGroup.Children[0] ];

	bool bHitTargetBefore = false;

	float MinError = RootCluster.LODError;

	if( SelectedGroupsMask )
	{
		SelectedGroupsMask->Init( false, Groups.Num() );
		(*SelectedGroupsMask)[ Groups.Num() - 1 ] = true;
	}
	
	FBinaryHeap< float > Heap;
	Heap.Add( -RootCluster.LODError, RootGroup.Children[0] );

	while( true )
	{
		// Grab highest error cluster to replace to reduce cut error
		const FCluster& Cluster = Clusters[ Heap.Top() ];

		if( Cluster.MipLevel == 0 )
			break;
		if( Cluster.GeneratingGroupIndex == MAX_uint32 )
			break;

		bool bHitTarget =
			Heap.Num() * FCluster::ClusterSize > TargetNumTris ||
			MinError < TargetError;

		// Overshoot the target by TargetOvershoot number of triangles. This allows granular edge collapses to better minimize error to the target.
		if( TargetOvershoot > 0 && bHitTarget && !bHitTargetBefore )
		{
			TargetNumTris = Heap.Num() * FCluster::ClusterSize + TargetOvershoot;
			bHitTarget = false;
			bHitTargetBefore = true;
		}

		if( bHitTarget && Cluster.LODError < MinError )
			break;
		
		Heap.Pop();

		check( Cluster.LODError <= MinError );
		MinError = Cluster.LODError;

		if( SelectedGroupsMask )
		{
			(*SelectedGroupsMask)[ Cluster.GeneratingGroupIndex ] = true;
		}

		for( uint32 Child : Groups[ Cluster.GeneratingGroupIndex ].Children )
		{
			if( !Heap.IsPresent( Child ) )
			{
				const FCluster& ChildCluster = Clusters[ Child ];

				check( ChildCluster.MipLevel < Cluster.MipLevel );
				check( ChildCluster.LODError <= MinError );
				Heap.Add( -ChildCluster.LODError, Child );
			}
		}
	}

	return Heap;
}

} // namespace Nanite