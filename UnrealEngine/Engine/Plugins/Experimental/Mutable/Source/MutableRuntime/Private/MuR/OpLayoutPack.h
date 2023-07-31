// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Layout.h"
#include "MuR/Platform.h"

#include <limits>


namespace mu
{

	//---------------------------------------------------------------------------------------------
    // inline void SimpleLayoutPack( Layout* pResult, const Layout* pSource )
	// {
    //     check( pResult->GetBlockCount() == pSource->GetBlockCount() );

	// 	int blockCount = pSource->GetBlockCount();
	// 	vector< vec2<int> > blocks( blockCount );

	// 	// Look for the maximum block sizes on the layout and the total area
	// 	int maxX = 0;
	// 	int maxY = 0;
	// 	int area = 0;
	// 	for ( int index=0; index<blockCount; ++index )
	// 	{
	// 		box< vec2<int> > b;
	// 		pSource->GetBlock( index, &b.min[0], &b.min[1], &b.size[0], &b.size[1] );

	// 		maxX = FMath::Max( maxX, b.size[0] );
	// 		maxY = FMath::Max( maxY, b.size[1] );

	// 		area += b.size[0] * b.size[1];

	// 		blocks[index] = b.size;
	// 	}

	// 	// Grow until the area is big enough to fit all blocks. We always grow X first, because
	// 	// in case we cannot pack everything, we will grow Y with the current horizon algorithm.
	// 	maxX = ceilPow2( maxX );
	// 	maxY = ceilPow2( maxY );
	// 	while ( maxX*maxY<area )
	// 	{
	// 		if (maxX>maxY)
	// 		{
	// 			maxY*=2;
	// 		}
	// 		else
	// 		{
	// 			maxX*=2;
	// 		}
	// 	}

	// 	// Sort by height, area
	// 	vector<int> sorted;
	// 	sorted.reserve( blockCount );
	// 	for ( int index=0; index<blockCount; ++index )
	// 	{
	// 		int thisHeight = blocks[index][1];
	// 		int thisArea = blocks[index][0] * blocks[index][1];

	// 		int p;
	// 		for ( p=0; p<(int)sorted.Num(); ++p )
	// 		{
	// 			int pos = sorted[p];
	// 			int posHeight = blocks[pos][1];
	// 			int posArea = blocks[pos][0] * blocks[pos][1];

	// 			if ( posHeight<thisHeight || ( posHeight==thisHeight && posArea<thisArea ))
	// 			{
	// 				break;
	// 			}
	// 		}

	// 		sorted.insert( sorted.begin()+p, index );
	// 	}

	// 	// Pack with fixed horizontal size
	// 	vector<int> horizon( maxX, 0 );
	// 	vector< vec2<int> > positions( blockCount );
	// 	maxY = 0;

	// 	for ( int p=0; p<(int)sorted.Num(); ++p )
	// 	{
	// 		int index = sorted[p];

	// 		// Seek for the lowest span where the block fits
	// 		int currentLevel = std::numeric_limits<int>::max();
	// 		int currentX = -1;
	// 		for ( int x=0; x<=maxX-blocks[index][0]; ++x )
	// 		{
	// 			int level = 0;
	// 			for( int xs=x; xs<x+blocks[index][0]; ++xs )
	// 			{
	// 				level = FMath::Max( level, horizon[xs] );
	// 			}

	// 			if (level<currentLevel)
	// 			{
	// 				currentLevel = level;
	// 				currentX = x;
	// 			}

	// 		}

	// 		check( currentX>=0 && currentX<=maxX-blocks[index][0] );

	// 		// Update horizon
	// 		for( int xs=currentX; xs<currentX+blocks[index][0]; ++xs )
	// 		{
	// 			horizon[xs] = currentLevel+blocks[index][1];
	// 		}

	// 		// Store
	// 		positions[ index ] = vec2<int>( currentX, currentLevel );
	// 		maxY = FMath::Max( maxY, currentLevel+blocks[index][1] );
	// 	}

    //     // Set data in the result
	// 	maxY = ceilPow2( maxY );
	// 	pResult->SetGridSize( maxX, maxY );
	// 	for ( int index=0; index<blockCount; ++index )
	// 	{
	// 		pResult->SetBlock
	// 			(
	// 				index,
	// 				positions[index][0], positions[index][1],
    //                 blocks[index][0], blocks[index][1]
	// 			);
	// 	}
	// }
	

	//---------------------------------------------------------------------------------------------
	//! Block comparison function to establish an order
	//---------------------------------------------------------------------------------------------
	struct LAY_BLOCK
	{
		LAY_BLOCK()
		{
			index = -1;
		}

		LAY_BLOCK( int i, vec2<int> s, int p=0 )
		{
			index = i;
			size = s;
			priority = p;
		}

		int index;
		vec2<int> size;
		int priority;
	};

    inline bool CompareBlocks( const LAY_BLOCK& a, const LAY_BLOCK& b )
    {
        if ( a.size[1]>b.size[1] )
        {
            return true;
        }
        else if ( a.size[1]<b.size[1] )
        {
            return false;
        }
        else
        {
            int areaA = a.size[0]*a.size[1];
            int areaB = b.size[0]*b.size[1];
            if (areaA > areaB)
            {
                return true;
            }
            else if (areaA < areaB)
            {
                return false;
            }
            else
            {
                // This has to be deterministic, and ids are supposed to be unique
                return a.index<b.index;
            }
        }
    }

	inline bool CompareBlocksPriority(const LAY_BLOCK& a, const LAY_BLOCK& b)
	{
		return a.priority > b.priority;
	}


    //---------------------------------------------------------------------------------------------
    //! More expensive but precise version
    //---------------------------------------------------------------------------------------------
    struct SCRATCH_LAYOUT_PACK
    {
        TArray< vec2<int> > blocks;
		TArray< LAY_BLOCK > sorted;
		TArray< vec2<int> > positions;
		TArray< int > priorities;
		TArray< vec2<int> > reductions;
    };


    inline string DebugGetBlockAt( const SCRATCH_LAYOUT_PACK& scratch,
                                   const TArray<uint8_t>& packedFlag,
                                   int x, int y )
    {
        for ( size_t b=0; b<packedFlag.Num(); ++b )
        {
            if (packedFlag[b])
            {
                auto i = scratch.sorted[b].index;
                if ( x>=scratch.positions[i][0] &&
                     x<scratch.positions[i][0]+scratch.sorted[b].size[0] &&
                     y>=scratch.positions[i][1] &&
                     y<scratch.positions[i][1]+scratch.sorted[b].size[1]
                     )
                {
                    char temp[2];
                    temp[1] = 0;
                    temp[0] = char('a')+char(b);
                    return temp;
                }
            }
        }
        return ".";
    }


    // inline void LayoutPack( Layout* pResult,
    //                         const Layout* pSourceLayout,
    //                         SCRATCH_LAYOUT_PACK* scratch  )
    // {
    //     check( pResult->GetBlockCount() == pSourceLayout->GetBlockCount() );

    //     int blockCount = pSourceLayout->GetBlockCount();
    //     check( (int)scratch->blocks.Num()==blockCount );

    //     // Look for the maximum block sizes on the layout and the total area
    //     int maxX = 0;
    //     int maxY = 0;
    //     int area = 0;
    //     for ( int index=0; index<blockCount; ++index )
    //     {
    //         box< vec2<int> > b;
    //         pSourceLayout->GetBlock( index, &b.min[0], &b.min[1], &b.size[0], &b.size[1] );

    //         maxX = FMath::Max( maxX, b.size[0] );
    //         maxY = FMath::Max( maxY, b.size[1] );

    //         area += b.size[0] * b.size[1];

    //         scratch->blocks[index] = b.size;
    //     }

    //     // Grow until the area is big enough to fit all blocks. We always grow X first, because
    //     // in case we cannot pack everything, we will grow Y with the current horizon algorithm.
    //     maxX = ceilPow2( maxX );
    //     maxY = ceilPow2( maxY );
    //     while ( maxX*maxY<area )
    //     {
    //         if (maxX>maxY)
    //         {
    //             maxY*=2;
    //         }
    //         else
    //         {
    //             maxX*=2;
    //         }
    //     }

    //     // Sort by height, area
    //     check( (int)scratch->sorted.Num()==blockCount );
    //     for ( int index=0; index<blockCount; ++index )
    //     {
    //         scratch->sorted[index] = LAY_BLOCK( index, scratch->blocks[index], scratch->priorities[index] );
    //     }
    //     std::sort( scratch->sorted.begin(), scratch->sorted.end(), CompareBlocks );

    //     check( (int)scratch->sorted.Num()==blockCount );

    //     bool fits = false;
    //     int iterations = 0;

    //     while (!fits)
    //     {
    //         ++iterations;

    //         // Pack with fixed horizontal size
    //         check( maxX<256 );
    //         int16_t horizon[256];
    //         mutable_memset( horizon, 0, 256*sizeof(int16_t) );
    //         maxY = 0;

    //         for ( size_t p=0; p<scratch->sorted.Num(); ++p )
    //         {
    //             // Seek for the lowest span where the block fits
    //             int currentLevel = std::numeric_limits<int>::max();
    //             int currentX = 0;
    //             for ( int x=0; x<=maxX-scratch->sorted[p].size[0]; ++x )
    //             {
    //                 int level = 0;
    //                 for( int xs=x; xs<x+scratch->sorted[p].size[0]; ++xs )
    //                 {
    //                     level = FMath::Max( level, (int)horizon[xs] );
    //                 }

    //                 if (level<currentLevel)
    //                 {
    //                     currentLevel = level;
    //                     currentX = x;
    //                 }

    //             }

    //             check( currentX>=0 && currentX<=maxX-scratch->sorted[p].size[0] );

    //             // Update horizon
    //             for( int xs=currentX; xs<currentX+scratch->sorted[p].size[0]; ++xs )
    //             {
    //                 horizon[xs] = (uint16)(currentLevel+scratch->sorted[p].size[1]);
    //             }

    //             // Store
    //             scratch->positions[ scratch->sorted[p].index ] = vec2<int>( currentX, currentLevel );
    //             maxY = FMath::Max( maxY, currentLevel+scratch->sorted[p].size[1] );
    //         }

    //         maxY = ceilPow2( maxY );

    //         // TODO: Adjust this value
    //         if ( maxY <= maxX || iterations>5000 )
    //         {
    //             fits = true;
    //         }
    //         else
    //         {
    //             fits = !std::next_permutation( scratch->sorted.begin(), scratch->sorted.end(), CompareBlocks );
    //         }
    //     }

    //     // Set data in the result
    //     pResult->SetGridSize( maxX, maxY );
    //     for ( int index=0; index<blockCount; ++index )
    //     {
    //         pResult->SetBlock
    //             (
    //                 index,
    //                 scratch->positions[index][0], scratch->positions[index][1],
    //                 scratch->blocks[index][0], scratch->blocks[index][1]
    //             );
    //     }
    // }



    //---------------------------------------------------------------------------------------------
    //! Even more expensive but precise version
    //---------------------------------------------------------------------------------------------
	inline void ReduceBlock(int blockCount, int* area, int* reverse_it, SCRATCH_LAYOUT_PACK* scratch)
	{
		int r_it = (*reverse_it);
		bool pass = false;

		int oldBlockArea = scratch->sorted[r_it].size[0] * scratch->sorted[r_it].size[1];

		if (scratch->sorted[r_it].size[0] != 1 || scratch->sorted[r_it].size[1] != 1)
		{
			if (scratch->reductions[scratch->sorted[r_it].index][0] > scratch->reductions[scratch->sorted[r_it].index][1])
			{
				if (scratch->sorted[r_it].size[1] > 1)
				{
					scratch->sorted[r_it].size[1] /= 2;
					scratch->blocks[scratch->sorted[r_it].index][1] /= 2;
					pass = true;
				}

				scratch->reductions[scratch->sorted[r_it].index][1] += 1;
			}
			else if (scratch->reductions[scratch->sorted[r_it].index][0] < scratch->reductions[scratch->sorted[r_it].index][1])
			{
				if (scratch->sorted[r_it].size[0] > 1)
				{
					scratch->sorted[r_it].size[0] /= 2;
					scratch->blocks[scratch->sorted[r_it].index][0] /= 2;
					pass = true;
				}

				scratch->reductions[scratch->sorted[r_it].index][0] += 1;
			}
			else
			{
				if (r_it % 2 == 0)
				{
					if (scratch->sorted[r_it].size[0] > 1)
					{
						scratch->sorted[r_it].size[0] /= 2;
						scratch->blocks[scratch->sorted[r_it].index][0] /= 2;
						pass = true;
					}

					scratch->reductions[scratch->sorted[r_it].index][0] += 1;
				}
				else
				{
					if (scratch->sorted[r_it].size[1] > 1)
					{
						scratch->sorted[r_it].size[1] /= 2;
						scratch->blocks[scratch->sorted[r_it].index][1] /= 2;
						pass = true;
					}

					scratch->reductions[scratch->sorted[r_it].index][1] += 1;
				}
			}
		}
		else
		{
			pass = true;
		}
		
		int newBlockArea = scratch->sorted[r_it].size[0] * scratch->sorted[r_it].size[1];

		(*area) = (*area) - (oldBlockArea - newBlockArea);

		if (pass)
		{
			(*reverse_it) = (*reverse_it) + 1;

			if ((*reverse_it) >= blockCount)
			{
				(*reverse_it) = 0;
			}
		}
	}

	inline bool SetPositions(int bestY,int layoutSizeY, int* maxX, int* maxY, SCRATCH_LAYOUT_PACK* scratch, EPackStrategy packStrategy)
	{
		bool fits = true;

		// Number of blocks alrady packed
		size_t packed = 0;
		TArray<uint8_t> packedFlag;
		packedFlag.SetNumZeroed(scratch->sorted.Num());

		// Pack with fixed horizontal size
		check(*maxX < 256);
		if (*maxX > 256) return true;

		int16_t horizon[256];
		FMemory::Memzero(horizon, 256 * sizeof(int16_t));
		*maxY = 0;

		int iterations = 0;

		while (packed < scratch->sorted.Num() || iterations > 5000)
		{
			++iterations;

			int best = -1;
			int bestX = -1;
			int bestLevel = -1;
			int bestWithHole = -1;
			int bestWithHoleX = -1;
			int bestWithHoleLevel = -1;
			for (size_t candidate = 0; candidate < scratch->sorted.Num(); ++candidate)
			{
				// Skip it if we packed it already
				if (packedFlag[candidate]) continue;

				auto candidateSizeX = scratch->sorted[candidate].size[0];
				auto candidateSizeY = scratch->sorted[candidate].size[1];

				// Seek for the lowest span where the block fits
				int currentLevel = std::numeric_limits<int>::max();
				int currentX = 0;
				int currentLevelWithoutHole = std::numeric_limits<int>::max();
				int currentXWithoutHole = 0;
				for (int x = 0; x <= *maxX - candidateSizeX; ++x)
				{
					int level = 0;
					for (int xs = x; xs < x + candidateSizeX; ++xs)
					{
						level = FMath::Max(level, (int)horizon[xs]);
					}

					if (level < currentLevel)
					{
						currentLevel = level;
						currentX = x;
					}

					// Does it make an unfillable hole with the top or side?
					int minX = std::numeric_limits<int>::max();
					int minY = std::numeric_limits<int>::max();
					for (size_t b = 0; b < scratch->sorted.Num(); ++b)
					{
						if (!packedFlag[b] && b != candidate)
						{
							minX = FMath::Min(minX, scratch->sorted[b].size[0]);
							minY = FMath::Min(minY, scratch->sorted[b].size[1]);
						}
					}

					bool hole =
						// Vertical unfillable gap
						(
						(minY != std::numeric_limits<int>::max())
							&&
							(currentLevel + candidateSizeY) < bestY
							&&
							(currentLevel + minY + candidateSizeY) > bestY
							)
						||
						// Horizontal unfillable gap
						(
						(minX != std::numeric_limits<int>::max())
							&&
							(currentX + candidateSizeX) < *maxX
							&&
							(currentX + minX + candidateSizeX) > *maxX
							);

					// Does it make a hole with the horizon?
					for (int xs = x; !hole && xs < x + scratch->sorted[candidate].size[0]; ++xs)
					{
						hole = (level > (int)horizon[xs]);
					}

					if (!hole && level < currentLevelWithoutHole)
					{
						currentLevelWithoutHole = level;
						currentXWithoutHole = x;
					}
				}

				if (currentLevelWithoutHole <= currentLevel)
				{
					best = int(candidate);
					bestX = currentXWithoutHole;
					bestLevel = currentLevelWithoutHole;
					break;
				}

				if (bestWithHole < 0)
				{
					bestWithHole = int(candidate);
					bestWithHoleX = currentX;
					bestWithHoleLevel = currentLevel;
				}
			}

			check(best >= 0 || bestWithHole >= 0);

			// If there is no other option, accept leaving a hole.
			if (best < 0)
			{
				best = bestWithHole;
				bestX = bestWithHoleX;
				bestLevel = bestWithHoleLevel;
			}

			if (bestX >= 0)
			{
				// Update horizon
				for (int xs = bestX; xs < bestX + scratch->sorted[best].size[0]; ++xs)
				{
					horizon[xs] = (uint16)(bestLevel + scratch->sorted[best].size[1]);
				}
			}

			// Store
			scratch->positions[scratch->sorted[best].index] = vec2<int>(bestX, bestLevel);
			*maxY = FMath::Max(*maxY, bestLevel + scratch->sorted[best].size[1]);

			if (packStrategy == EPackStrategy::FIXED_LAYOUT && *maxY > layoutSizeY)
			{
				fits = false;
				break;
			}

			packedFlag[best] = 1;
			packed++;

			//                for (int y=0; y<*maxY; ++y)
			//                {
			//                    string line;
			//                    for (int x=0;x<*maxX;++x)
			//                    {
			//                        line += DebugGetBlockAt( *scratch, packedFlag, x, y );
			//                    }
			//                    AXE_LOG("layout",Warning,line.c_str());
			//                }
			//                AXE_LOG("layout",Warning,"--------------------------------------------------");
		}

		if (packed < scratch->sorted.Num())
		{
			fits = false;
		}

		*maxY = ceilPow2(*maxY);

		return fits;
	}


    inline void LayoutPack3( Layout* pResult,
                             const Layout* pSourceLayout,
                             SCRATCH_LAYOUT_PACK* scratch  )
    {
		MUTABLE_CPUPROFILER_SCOPE(MeshLayoutPack3);

        check( pResult->GetBlockCount() == pSourceLayout->GetBlockCount() );

        int blockCount = pSourceLayout->GetBlockCount();
        check( (int)scratch->blocks.Num()==blockCount );

		//Getting maximum layout grid size:
		int layoutSizeX, layoutSizeY;
		pSourceLayout->GetMaxGridSize(&layoutSizeX, &layoutSizeY);

		bool usePriority = false;

        // Look for the maximum block sizes on the layout and the total area
        int maxX = 0;
        int maxY = 0;
        int area = 0;
        for ( int index=0; index<blockCount; ++index )
        {
            box< vec2<int> > b;
            pSourceLayout->GetBlock( index, &b.min[0], &b.min[1], &b.size[0], &b.size[1] );

			int p;
			pSourceLayout->GetBlockPriority(index, &p);

			vec2<int> reductions;
			reductions[0] = 0;
			reductions[1] = 0;

			if (p > 0)
			{
				usePriority = true;
			}

            maxX = FMath::Max( maxX, b.size[0] );
            maxY = FMath::Max( maxY, b.size[1] );

            area += b.size[0] * b.size[1];

            scratch->blocks[index] = b.size;
			scratch->priorities[index] = p;
			scratch->reductions[index] = reductions;
        }


        // Grow until the area is big enough to fit all blocks. We always grow X first, because
        // in case we cannot pack everything, we will grow Y with the current horizon algorithm.
		if (pSourceLayout->GetLayoutPackingStrategy() == EPackStrategy::RESIZABLE_LAYOUT)
		{
			maxX = ceilPow2(maxX);
			maxY = ceilPow2(maxY);

			while (maxX*maxY < area)
			{
				if (maxX > maxY)
				{
					maxY *= 2;
				}
				else
				{
					maxX *= 2;
				}
			}
		}
		else
		{
			//Increase the maximum layout size if the grid area is smaller than the number of blocks
			while (blockCount > layoutSizeX*layoutSizeY)
			{
				if (layoutSizeX > layoutSizeY)
				{
					layoutSizeY *= 2;

					if (layoutSizeY == 0)
					{
						layoutSizeY = 1;
					}
				}
				else
				{
					layoutSizeX *= 2;

					if (layoutSizeX == 0)
					{
						layoutSizeX = 1;
					}
				}
			}

			//The highest block is bigger than the layout max size
			while (maxX > layoutSizeX)
			{
				area = 0;

				for (int index = 0; index < blockCount; ++index)
				{
					if (scratch->blocks[index][0] == maxX)
					{
						scratch->blocks[index][0] /= 2;
						scratch->reductions[index][0]++;
					}
				}
				maxX = 0;
				
				//recalculating area and maximum block sizes
				for (int index = 0; index < blockCount; ++index)
				{
					maxX = FMath::Max(maxX, scratch->blocks[index][0]);
					area += scratch->blocks[index][0] * scratch->blocks[index][1];
				}
			}

			while (maxY > layoutSizeY)
			{
				area = 0;

				for (int index = 0; index < blockCount; ++index)
				{
					if (scratch->blocks[index][1] == maxY)
					{
						scratch->blocks[index][1] /= 2;
						scratch->reductions[index][1]++;
					}
				}
				maxY = 0;

				//recalculating area and maximum block sizes
				for (int index = 0; index < blockCount; ++index)
				{
					maxY = FMath::Max(maxY, scratch->blocks[index][1]);
					area += scratch->blocks[index][0] * scratch->blocks[index][1];
				}
			}

			maxX = ceilPow2(maxX);
			maxY = ceilPow2(maxY);

			// Grow until the area is big enough to fit all blocks or the size is equal to the max layout size.
			while (maxX*maxY < area && (maxX < layoutSizeX || maxY < layoutSizeY))
			{
				if (maxX > maxY)
				{
					maxY *= 2;
				}
				else
				{
					maxX *= 2;
				}
			}
		}
        
        int bestY = maxY;
		int r_it = 0;

        // Sort by height, area
        check( (int)scratch->sorted.Num()==blockCount );
        for ( int index=0; index<blockCount; ++index )
        {
            scratch->sorted[index] = LAY_BLOCK( index, scratch->blocks[index], scratch->priorities[index] );
        }

		scratch->sorted.Sort( CompareBlocks);
		
		if(pSourceLayout->GetLayoutPackingStrategy() == EPackStrategy::FIXED_LAYOUT)
		{
			if (usePriority)	//Sort by priority
			{
				scratch->sorted.Sort(CompareBlocksPriority);
			}

			//Shrink blocks in case we do not have enough space to pack everything
			while (maxX*maxY < area)
			{
				ReduceBlock(blockCount, &area, &r_it, scratch);
			}
			
		}

		bool fits = false;

		while (!fits)
		{
			//Sort by height&area before packing
			if (usePriority)
			{
				scratch->sorted.Sort(CompareBlocks);
			}

			//Try to pack everything
			fits = SetPositions(bestY, layoutSizeY, &maxX, &maxY, scratch, pSourceLayout->GetLayoutPackingStrategy());

			if (!fits && pSourceLayout->GetLayoutPackingStrategy() == EPackStrategy::FIXED_LAYOUT)
			{
				//Sort by priority before shrink
				if (usePriority)
				{
					scratch->sorted.Sort(CompareBlocksPriority);
				}

				ReduceBlock(blockCount, &area, &r_it, scratch);
			}
		}

        // Set data in the result
        pResult->SetGridSize( maxX, maxY );
        pResult->SetMaxGridSize(layoutSizeX, layoutSizeY);
		pResult->SetLayoutPackingStrategy(pSourceLayout->GetLayoutPackingStrategy());

        for ( int index=0; index<blockCount; ++index )
        {
            pResult->SetBlock
                (
                    index,
                    scratch->positions[index][0], scratch->positions[index][1],
                    scratch->blocks[index][0], scratch->blocks[index][1]
                );

			pResult->SetBlockPriority(index, scratch->priorities[index]);
        }
    }

}
