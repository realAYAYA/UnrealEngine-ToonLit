// Copyright Epic Games, Inc. All Rights Reserved.

#include "GraphPartitioner.h"
#include "Async/Async.h"
#include "Async/LocalWorkQueue.h"
#include "EngineLogs.h"
#include "HAL/PlatformMemory.h"

FGraphPartitioner::FGraphPartitioner( uint32 InNumElements )
	: NumElements( InNumElements )
{
	Indexes.AddUninitialized( NumElements );
	for( uint32 i = 0; i < NumElements; i++ )
	{
		Indexes[i] = i;
	}
}

FGraphPartitioner::FGraphData* FGraphPartitioner::NewGraph( uint32 NumAdjacency ) const
{
	NumAdjacency += LocalityLinks.Num();

	FGraphData* RESTRICT Graph = new FGraphPartitioner::FGraphData;
	Graph->Offset = 0;
	Graph->Num = NumElements;
	Graph->Adjacency.Reserve( NumAdjacency );
	Graph->AdjacencyCost.Reserve( NumAdjacency );
	Graph->AdjacencyOffset.AddUninitialized( NumElements + 1 );
	return Graph;
}

void FGraphPartitioner::Partition( FGraphData* Graph, int32 InMinPartitionSize, int32 InMaxPartitionSize )
{
	MinPartitionSize = InMinPartitionSize;
	MaxPartitionSize = InMaxPartitionSize;

	const int32 TargetPartitionSize = ( MinPartitionSize + MaxPartitionSize ) / 2;
	const int32 TargetNumPartitions = FMath::DivideAndRoundUp( Graph->Num, TargetPartitionSize );

	if( TargetNumPartitions > 1 )
	{
		PartitionIDs.AddUninitialized( NumElements );

		idx_t NumConstraints = 1;
		idx_t NumParts = TargetNumPartitions;
		idx_t EdgesCut = 0;

		idx_t Options[ METIS_NOPTIONS ];
		METIS_SetDefaultOptions( Options );

		Options[ METIS_OPTION_UFACTOR ] = 200;//( 1000 * MaxPartitionSize * TargetNumPartitions ) / NumElements - 1000;
		//Options[ METIS_OPTION_NCUTS ] = 8;
		//Options[ METIS_OPTION_IPTYPE ] = METIS_IPTYPE_RANDOM;
		//Options[ METIS_OPTION_SEED ] = 17;

		//int r = METIS_PartGraphRecursive(
		int r = METIS_PartGraphKway(
			&Graph->Num,
			&NumConstraints,			// number of balancing constraints
			Graph->AdjacencyOffset.GetData(),
			Graph->Adjacency.GetData(),
			NULL,						// Vert weights
			NULL,						// Vert sizes for computing the total communication volume
			Graph->AdjacencyCost.GetData(),	// Edge weights
			&NumParts,
			NULL,						// Target partition weight
			NULL,						// Allowed load imbalance tolerance
			Options,
			&EdgesCut,
			PartitionIDs.GetData()
		);

		if (r == METIS_ERROR_MEMORY)
		{
			UE_LOG(LogStaticMesh, Error, TEXT("Call to METIS_PartGraphKway() failed - error code: %d, Graph->Num: %d"), r, Graph->Num);
			// We can't get the precise allocation size, but Metis logs an error that contains the actual error.
			FPlatformMemory::OnOutOfMemory(0, 0);
		}

		if( ensure( r == METIS_OK ) )
		{
			TArray< uint32 > ElementCount;
			ElementCount.AddZeroed( TargetNumPartitions );

			for( uint32 i = 0; i < NumElements; i++ )
			{
				ElementCount[ PartitionIDs[i] ]++;
			}

			uint32 Begin = 0;
			Ranges.AddUninitialized( TargetNumPartitions );
			for( int32 PartitionIndex = 0; PartitionIndex < TargetNumPartitions; PartitionIndex++ )
			{
				Ranges[ PartitionIndex ] = { Begin, Begin + ElementCount[ PartitionIndex ] };
				Begin += ElementCount[ PartitionIndex ];
				ElementCount[ PartitionIndex ] = 0;
			}

			TArray< uint32 > OldIndexes;
			Swap( Indexes, OldIndexes );
			
			Indexes.AddUninitialized( NumElements );
			for( uint32 i = 0; i < NumElements; i++ )
			{
				uint32 PartitionIndex = PartitionIDs[i];
				uint32 Offset = Ranges[ PartitionIndex ].Begin;
				uint32 Num = ElementCount[ PartitionIndex ]++;

				Indexes[ Offset + Num ] = OldIndexes[i];
			}
			
			PartitionIDs.Empty();
		}
	}
	else
	{
		// Single
		Ranges.Add( { 0, NumElements } );
	}

	for( uint32 i = 0; i < NumElements; i++ )
	{
		SortedTo[ Indexes[i] ] = i;
	}
}

void FGraphPartitioner::BisectGraph( FGraphData* Graph, FGraphData* ChildGraphs[2] )
{
	ChildGraphs[0] = nullptr;
	ChildGraphs[1] = nullptr;

	auto AddPartition =
		[ this ]( int32 Offset, int32 Num )
		{
			FRange& Range = Ranges[ NumPartitions++ ];
			Range.Begin	= Offset;
			Range.End	= Offset + Num;
		};

	if( Graph->Num <= MaxPartitionSize )
	{
		AddPartition( Graph->Offset, Graph->Num );
		return;
	}

	const int32 TargetPartitionSize = ( MinPartitionSize + MaxPartitionSize ) / 2;
	const int32 TargetNumPartitions = FMath::Max( 2, FMath::DivideAndRoundNearest( Graph->Num, TargetPartitionSize ) );

	check( Graph->AdjacencyOffset.Num() == Graph->Num + 1 );

	idx_t NumConstraints = 1;
	idx_t NumParts = 2;
	idx_t EdgesCut = 0;

	real_t PartitionWeights[] = {
		float( TargetNumPartitions / 2 ) / TargetNumPartitions,
		1.0f - float( TargetNumPartitions / 2 ) / TargetNumPartitions
	};

	idx_t Options[ METIS_NOPTIONS ];
	METIS_SetDefaultOptions( Options );

	// Allow looser tolerance when at the higher levels. Strict balance isn't that important until it gets closer to partition sized.
	bool bLoose = TargetNumPartitions >= 128 || MaxPartitionSize / MinPartitionSize > 1;
	bool bSlow = Graph->Num < 4096;
	
	Options[ METIS_OPTION_UFACTOR ] = bLoose ? 200 : 1;
	//Options[ METIS_OPTION_NCUTS ] = Graph->Num < 1024 ? 8 : ( Graph->Num < 4096 ? 4 : 1 );
	//Options[ METIS_OPTION_NCUTS ] = bSlow ? 4 : 1;
	//Options[ METIS_OPTION_NITER ] = bSlow ? 20 : 10;
	//Options[ METIS_OPTION_IPTYPE ] = METIS_IPTYPE_RANDOM;
	//Options[ METIS_OPTION_MINCONN ] = 1;

	int r = METIS_PartGraphRecursive(
		&Graph->Num,
		&NumConstraints,			// number of balancing constraints
		Graph->AdjacencyOffset.GetData(),
		Graph->Adjacency.GetData(),
		NULL,						// Vert weights
		NULL,						// Vert sizes for computing the total communication volume
		Graph->AdjacencyCost.GetData(),	// Edge weights
		&NumParts,
		PartitionWeights,			// Target partition weight
		NULL,						// Allowed load imbalance tolerance
		Options,
		&EdgesCut,
		PartitionIDs.GetData() + Graph->Offset
	);

	if (r == METIS_ERROR_MEMORY)
	{
		UE_LOG(LogStaticMesh, Error, TEXT("Call to METIS_PartGraphRecursive() failed - error code: %d, Graph->Num: %d"), r, Graph->Num);
		// We can't get the precise allocation size, but Metis logs an error that contains the actual error.
		FPlatformMemory::OnOutOfMemory(0, 0);
	}

	checkf(r == METIS_OK, TEXT("Call to METIS_PartGraphRecursive() failed - error code: %d, Graph->Num: %d"), r, Graph->Num);

	{
		// In place divide the array
		// Both sides remain sorted but back is reversed.
		int32 Front = Graph->Offset;
		int32 Back =  Graph->Offset + Graph->Num - 1;
		while( Front <= Back )
		{
			while( Front <= Back && PartitionIDs[ Front ] == 0 )
			{
				SwappedWith[ Front ] = Front;
				Front++;
			}
			while( Front <= Back && PartitionIDs[ Back ] == 1 )
			{
				SwappedWith[ Back ] = Back;
				Back--;
			}

			if( Front < Back )
			{
				Swap( Indexes[ Front ], Indexes[ Back ] );

				SwappedWith[ Front ] = Back;
				SwappedWith[ Back ] = Front;
				Front++;
				Back--;
			}
		}

		int32 Split = Front;

		int32 Num[2];
		Num[0] = Split - Graph->Offset;
		Num[1] = Graph->Offset + Graph->Num - Split;
				
		check( Num[0] > 1 );
		check( Num[1] > 1 );

		if( Num[0] <= MaxPartitionSize && Num[1] <= MaxPartitionSize )
		{
			AddPartition( Graph->Offset,	Num[0] );
			AddPartition( Split,			Num[1] );
		}
		else
		{
			for( int32 i = 0; i < 2; i++ )
			{
				ChildGraphs[i] = new FGraphData;
				ChildGraphs[i]->Adjacency.Reserve( Graph->Adjacency.Num() >> 1 );
				ChildGraphs[i]->AdjacencyCost.Reserve( Graph->Adjacency.Num() >> 1 );
				ChildGraphs[i]->AdjacencyOffset.Reserve( Num[i] + 1 );
				ChildGraphs[i]->Num = Num[i];
			}

			ChildGraphs[0]->Offset = Graph->Offset;
			ChildGraphs[1]->Offset = Split;

			for( int32 i = 0; i < Graph->Num; i++ )
			{
				FGraphData* ChildGraph = ChildGraphs[ i >= ChildGraphs[0]->Num ];

				ChildGraph->AdjacencyOffset.Add( ChildGraph->Adjacency.Num() );
				
				int32 OrgIndex = SwappedWith[ Graph->Offset + i ] - Graph->Offset;
				for( idx_t AdjIndex = Graph->AdjacencyOffset[ OrgIndex ]; AdjIndex < Graph->AdjacencyOffset[ OrgIndex + 1 ]; AdjIndex++ )
				{
					idx_t Adj     = Graph->Adjacency[ AdjIndex ];
					idx_t AdjCost = Graph->AdjacencyCost[ AdjIndex ];

					// Remap to child
					Adj = SwappedWith[ Graph->Offset + Adj ] - ChildGraph->Offset;

					// Edge connects to node in this graph
					if( 0 <= Adj && Adj < ChildGraph->Num )
					{
						ChildGraph->Adjacency.Add( Adj );
						ChildGraph->AdjacencyCost.Add( AdjCost );
					}
				}
			}
			ChildGraphs[0]->AdjacencyOffset.Add( ChildGraphs[0]->Adjacency.Num() );
			ChildGraphs[1]->AdjacencyOffset.Add( ChildGraphs[1]->Adjacency.Num() );
		}
	}
}

void FGraphPartitioner::RecursiveBisectGraph( FGraphData* Graph )
{
	FGraphData* ChildGraphs[2];
	BisectGraph( Graph, ChildGraphs );
	delete Graph;

	if( ChildGraphs[0] && ChildGraphs[1] )
	{
		RecursiveBisectGraph( ChildGraphs[0] );
		RecursiveBisectGraph( ChildGraphs[1] );
	}
}

void FGraphPartitioner::PartitionStrict( FGraphData* Graph, int32 InMinPartitionSize, int32 InMaxPartitionSize, bool bThreaded )
{
	MinPartitionSize = InMinPartitionSize;
	MaxPartitionSize = InMaxPartitionSize;

	PartitionIDs.AddUninitialized( NumElements );
	SwappedWith.AddUninitialized( NumElements );

	// Adding to atomically so size big enough to not need to grow.
	int32 NumPartitionsExpected = FMath::DivideAndRoundUp( Graph->Num, MinPartitionSize );
	Ranges.AddUninitialized( NumPartitionsExpected * 2 );
	NumPartitions = 0;

	if( bThreaded && NumPartitionsExpected > 4 )
	{	
		extern CORE_API int32 GUseNewTaskBackend;
		if (GUseNewTaskBackend)
		{
			TLocalWorkQueue<FGraphData> LocalWork(Graph);
			LocalWork.Run(MakeYCombinator([this, &LocalWork](auto Self, FGraphData* Graph) -> void
			{
				FGraphData* ChildGraphs[2];
				BisectGraph( Graph, ChildGraphs );
				delete Graph;

				if( ChildGraphs[0] && ChildGraphs[1] )
				{
					// Only spawn add a worker thread if remaining work is expected to be large enough
					if (ChildGraphs[0]->Num > 256)
					{
						LocalWork.AddTask(ChildGraphs[0]);
						LocalWork.AddWorkers(1);
					}
					else
					{
						Self(ChildGraphs[0]);
					}
					Self(ChildGraphs[1]);
				}
			}));
		}
		else
		{
			const ENamedThreads::Type DesiredThread = IsInGameThread() ? ENamedThreads::AnyThread : ENamedThreads::AnyBackgroundThreadNormalTask;

			class FBuildTask
			{
			public:
				FBuildTask( FGraphPartitioner* InPartitioner, FGraphData* InGraph, ENamedThreads::Type InDesiredThread)
					: Partitioner( InPartitioner )
					, Graph( InGraph )
					, DesiredThread( InDesiredThread )
				{}

				void DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionEvent )
				{
					FGraphData* ChildGraphs[2];
					Partitioner->BisectGraph( Graph, ChildGraphs );
					delete Graph;

					if( ChildGraphs[0] && ChildGraphs[1] )
					{
						if( ChildGraphs[0]->Num > 256 )
						{
							FGraphEventRef Task = TGraphTask< FBuildTask >::CreateTask().ConstructAndDispatchWhenReady( Partitioner, ChildGraphs[0], DesiredThread);
							MyCompletionEvent->DontCompleteUntil( Task );
						}
						else
						{
							FBuildTask( Partitioner, ChildGraphs[0], DesiredThread).DoTask( CurrentThread, MyCompletionEvent );
						}

						FBuildTask( Partitioner, ChildGraphs[1], DesiredThread).DoTask( CurrentThread, MyCompletionEvent );
					}
				}

				static FORCEINLINE TStatId GetStatId()
				{
					RETURN_QUICK_DECLARE_CYCLE_STAT(FBuildTask, STATGROUP_ThreadPoolAsyncTasks);
				}

				static FORCEINLINE ESubsequentsMode::Type	GetSubsequentsMode()	{ return ESubsequentsMode::TrackSubsequents; }

				FORCEINLINE ENamedThreads::Type GetDesiredThread() const
				{
					return DesiredThread;
				}

			private:
				FGraphPartitioner*  Partitioner;
				FGraphData*         Graph;
				ENamedThreads::Type DesiredThread;
			};

			FGraphEventRef BuildTask = TGraphTask< FBuildTask >::CreateTask( nullptr ).ConstructAndDispatchWhenReady( this, Graph, DesiredThread);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes( BuildTask );
		}
	}
	else
	{
		RecursiveBisectGraph( Graph );
	}

	Ranges.SetNum( NumPartitions );

	if( bThreaded )
	{
		// Force a deterministic order
		Ranges.Sort();
	}

	PartitionIDs.Empty();
	SwappedWith.Empty();

	for( uint32 i = 0; i < NumElements; i++ )
	{
		SortedTo[ Indexes[i] ] = i;
	}
}