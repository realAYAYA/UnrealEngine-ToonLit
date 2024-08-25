// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Layout.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Platform.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include <string>

namespace mu
{

	//---------------------------------------------------------------------------------------------
	struct FLayoutBlock
	{
		FLayoutBlock()
		{
			index = -1;
		}

		FLayoutBlock( int32 i, UE::Math::TIntVector2<uint16> s, int32 p=0, bool bInReduceBothAxes = false, bool bInReduceByTwo = false )
		{
			index = i;
			size = s;
			priority = p;
			bReduceBothAxes = bInReduceBothAxes;
			bReduceByTwo = bInReduceByTwo;
		}

		int32 index;
		UE::Math::TIntVector2<uint16> size = UE::Math::TIntVector2<uint16>(0,0);
		int32 priority;
		bool bReduceBothAxes;
		bool bReduceByTwo;
	};

    inline bool CompareBlocks( const FLayoutBlock& a, const FLayoutBlock& b )
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

	inline bool CompareBlocksPriority(const FLayoutBlock& a, const FLayoutBlock& b)
	{
		if (a.priority == b.priority)
		{
			// TODO(Max): Check if this comparison modifies the block order while reducing them.
			return CompareBlocks(a, b);
		}

		return a.priority > b.priority;
	}


    //---------------------------------------------------------------------------------------------
    //! More expensive but precise version
    //---------------------------------------------------------------------------------------------
    struct SCRATCH_LAYOUT_PACK
    {
        TArray< UE::Math::TIntVector2<uint16> > blocks;
		TArray< FLayoutBlock > sorted;
		TArray< FIntVector2 > positions;
		TArray< int32 > priorities;
		TArray< FIntVector2 > reductions;
		TArray< int32 > ReduceBothAxes;
		TArray< int32 > ReduceByTwo;
    };


    inline char DebugGetBlockAt( const SCRATCH_LAYOUT_PACK& scratch,
                                   const TArray<uint8_t>& packedFlag,
                                   int x, int y )
    {
        for ( size_t b=0; b<packedFlag.Num(); ++b )
        {
            if (packedFlag[b])
            {
				int32 i = scratch.sorted[b].index;
                if ( x>=scratch.positions[i][0] &&
                     x<scratch.positions[i][0]+scratch.sorted[b].size[0] &&
                     y>=scratch.positions[i][1] &&
                     y<scratch.positions[i][1]+scratch.sorted[b].size[1]
                     )
                {
					return char('a')+char(b);
                }
            }
        }
        return '.';
    }


	inline void ReductionOperation(uint16& BlockSize, EReductionMethod ReductionMethod, int32 bReduceByTwo)
	{
		if (ReductionMethod == EReductionMethod::UNITARY_REDUCTION)
		{
			int32 Reduction = (bReduceByTwo && BlockSize > 2) ? 2 : 1;
			BlockSize -= Reduction;
			return;
		}
		
		// Reduce size by half
		BlockSize /= 2;
	}


    /** Updates the area, and the iterator. */
	inline void ReduceBlock(int blockCount, int32& InOutArea, int32& InOutBlockIt, SCRATCH_LAYOUT_PACK* scratch, EReductionMethod ReductionMethod)
	{
		int r_it = InOutBlockIt;
		bool pass = false;

		int oldBlockArea = scratch->sorted[r_it].size[0] * scratch->sorted[r_it].size[1];

		if (scratch->sorted[r_it].size[0] != 1 || scratch->sorted[r_it].size[1] != 1)
		{
			if (scratch->sorted[r_it].bReduceBothAxes)
			{
				// We reduce both sides of the block at the same time
				for (int32 Index = 0; Index <= 1; ++Index)
				{
					if (scratch->sorted[r_it].size[Index] > 1)
					{
						ReductionOperation(scratch->sorted[r_it].size[Index], ReductionMethod, scratch->sorted[r_it].bReduceByTwo);
						ReductionOperation(scratch->blocks[scratch->sorted[r_it].index][Index], ReductionMethod, scratch->sorted[r_it].bReduceByTwo );

						pass = true;
					}
				}
			}
			else
			{
				if (scratch->reductions[scratch->sorted[r_it].index][0] > scratch->reductions[scratch->sorted[r_it].index][1])
				{
					if (scratch->sorted[r_it].size[1] > 1)
					{
						ReductionOperation(scratch->sorted[r_it].size[1], ReductionMethod, scratch->sorted[r_it].bReduceByTwo);
						ReductionOperation(scratch->blocks[scratch->sorted[r_it].index][1], ReductionMethod, scratch->sorted[r_it].bReduceByTwo);
						pass = true;
					}

					scratch->reductions[scratch->sorted[r_it].index][1] += 1;
				}
				else if (scratch->reductions[scratch->sorted[r_it].index][0] < scratch->reductions[scratch->sorted[r_it].index][1])
				{
					if (scratch->sorted[r_it].size[0] > 1)
					{
						ReductionOperation(scratch->sorted[r_it].size[0], ReductionMethod, scratch->sorted[r_it].bReduceByTwo);
						ReductionOperation(scratch->blocks[scratch->sorted[r_it].index][0], ReductionMethod, scratch->sorted[r_it].bReduceByTwo);
						pass = true;
					}

					scratch->reductions[scratch->sorted[r_it].index][0] += 1;
				}
				else
				{
					// we select the first side to reduce "randomly"
					int32 Index = r_it % 2;

					// if we can't reduce a dimension then we try to reduce the other one
					if (scratch->sorted[r_it].size[Index] <= 1)
					{
						Index = r_it == 0 ? 1 : 0;
					}

					if (scratch->sorted[r_it].size[Index] > 1)
					{
						ReductionOperation(scratch->sorted[r_it].size[Index], ReductionMethod, scratch->sorted[r_it].bReduceByTwo);
						ReductionOperation(scratch->blocks[scratch->sorted[r_it].index][Index], ReductionMethod, scratch->sorted[r_it].bReduceByTwo);
						pass = true;
					}

					scratch->reductions[scratch->sorted[r_it].index][Index] += 1;
				}
			}
		}
		else
		{
			pass = true;
		}
		
		int newBlockArea = scratch->sorted[r_it].size[0] * scratch->sorted[r_it].size[1];

		InOutArea = InOutArea - (oldBlockArea - newBlockArea);

		if (pass)
		{
			InOutBlockIt = InOutBlockIt + 1;

			if (InOutBlockIt >= blockCount)
			{
				InOutBlockIt = 0;
			}
		}
	}


	inline bool SetPositions(int bestY,int layoutSizeY, uint16* maxX, uint16* maxY, SCRATCH_LAYOUT_PACK* scratch, EPackStrategy packStrategy)
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
				int currentLevel = TNumericLimits<int>::Max();
				int currentX = 0;
				int currentLevelWithoutHole = TNumericLimits<int>::Max();
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
					uint16 minX = TNumericLimits<uint16>::Max();
					uint16 minY = TNumericLimits<uint16>::Max();
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
						(minY != TNumericLimits<int>::Max())
							&&
							(currentLevel + candidateSizeY) < bestY
							&&
							(currentLevel + minY + candidateSizeY) > bestY
							)
						||
						// Horizontal unfillable gap
						(
						(minX != TNumericLimits<int>::Max())
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
			scratch->positions[scratch->sorted[best].index] = FIntVector2(bestX, bestLevel);
			*maxY = FMath::Max(*maxY, uint16(bestLevel + scratch->sorted[best].size[1]) );

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

		*maxY = FGenericPlatformMath::RoundUpToPowerOfTwo(*maxY);

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

		EPackStrategy LayoutStrategy = pSourceLayout->GetLayoutPackingStrategy();
		EReductionMethod ReductionMethod = pSourceLayout->GetBlockReductionMethod();

        // Look for the maximum block sizes on the layout and the total area
		uint16 maxX = 0;
		uint16 maxY = 0;
        int area = 0;

        for ( int index=0; index<blockCount; ++index )
        {
            box< UE::Math::TIntVector2<uint16> > b;
            pSourceLayout->GetBlock( index, &b.min[0], &b.min[1], &b.size[0], &b.size[1] );

			int p;
			bool bReduceBothAxes, bReduceByTwo;
			pSourceLayout->GetBlockOptions(index, p, bReduceBothAxes, bReduceByTwo);

			FIntVector2 reductions;
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
			scratch->ReduceBothAxes[index] = (int)bReduceBothAxes;
			scratch->ReduceByTwo[index] = (int)bReduceByTwo;
        }


        // Grow until the area is big enough to fit all blocks. We always grow X first, because
        // in case we cannot pack everything, we will grow Y with the current horizon algorithm.
		if (LayoutStrategy == EPackStrategy::RESIZABLE_LAYOUT)
		{
			maxX = FGenericPlatformMath::RoundUpToPowerOfTwo(maxX);
			maxY = FGenericPlatformMath::RoundUpToPowerOfTwo(maxY);

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

			// Reducing blocks that do not fit in the layout grid
			while (maxX > layoutSizeX)
			{
				area = 0;

				for (int index = 0; index < blockCount; ++index)
				{
					if (scratch->blocks[index][0] == maxX)
					{
						ReductionOperation(scratch->blocks[index][0], ReductionMethod, scratch->ReduceByTwo[index]);
						scratch->reductions[index][0]++;

						if (scratch->ReduceBothAxes[index] == 1)
						{
							ReductionOperation(scratch->blocks[index][1], ReductionMethod, scratch->ReduceByTwo[index]);
							scratch->reductions[index][1]++;
						}
					}
				}

				maxX = maxY = 0;
				
				//recalculating area and maximum block sizes
				for (int index = 0; index < blockCount; ++index)
				{
					maxX = FMath::Max(maxX, scratch->blocks[index][0]);

					// maxY could have been modified if a block had symmetry enabled in the previous reduction
					maxY = FMath::Max(maxY, scratch->blocks[index][1]);

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
						ReductionOperation(scratch->blocks[index][1], ReductionMethod, scratch->ReduceByTwo[index]);
						scratch->reductions[index][1]++;

						if (scratch->ReduceBothAxes[index] == 1)
						{
							ReductionOperation(scratch->blocks[index][0], ReductionMethod, scratch->ReduceByTwo[index]);
							scratch->reductions[index][0]++;
						}
					}
				}

				maxX = maxY = 0;

				//recalculating area and maximum block sizes
				for (int index = 0; index < blockCount; ++index)
				{
					maxY = FMath::Max(maxY, scratch->blocks[index][1]);

					// maxX could have been modified if a block had symmetry enabled in the previous reduction
					maxX = FMath::Max(maxX, scratch->blocks[index][0]);

					area += scratch->blocks[index][0] * scratch->blocks[index][1];
				}
			}

			maxX = FGenericPlatformMath::RoundUpToPowerOfTwo(maxX);
			maxY = FGenericPlatformMath::RoundUpToPowerOfTwo(maxY);

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

		// This is used to iterate through blocks.
		int32 BlockIterator = 0;

		// Making a copy of the blocks array to sort them
        check( (int)scratch->sorted.Num()==blockCount );
        for ( int index=0; index<blockCount; ++index )
        {
            scratch->sorted[index] = FLayoutBlock( index, scratch->blocks[index], scratch->priorities[index], (bool)scratch->ReduceBothAxes[index], (bool)scratch->ReduceByTwo[index]);
        }

		// Sort blocks by height, area
		scratch->sorted.Sort(CompareBlocks);

		if(LayoutStrategy == EPackStrategy::FIXED_LAYOUT)
		{
			if (usePriority)
			{
				//Sort blocks by priority
				scratch->sorted.Sort(CompareBlocksPriority);
			}

			// Shrink blocks in case we do not have enough space to pack everything
			while (maxX*maxY < area)
			{
				ReduceBlock(blockCount, area, BlockIterator, scratch, ReductionMethod);
			}
		}

		bool fits = false;

		while (!fits)
		{
			// Sort by height&area before packing
			if (usePriority)
			{
				scratch->sorted.Sort(CompareBlocks);
			}

			// Try to pack everything
			fits = SetPositions(bestY, layoutSizeY, &maxX, &maxY, scratch, LayoutStrategy);

			if (!fits && LayoutStrategy == EPackStrategy::FIXED_LAYOUT)
			{
				// Sort by priority before shrink
				if (usePriority)
				{
					scratch->sorted.Sort(CompareBlocksPriority);
				}

				ReduceBlock(blockCount, area, BlockIterator, scratch, ReductionMethod);
			}
		}

        // Set data in the result
        pResult->SetGridSize( maxX, maxY );
        pResult->SetMaxGridSize(layoutSizeX, layoutSizeY);
		pResult->SetLayoutPackingStrategy(LayoutStrategy);
		pResult->SetBlockReductionMethod(ReductionMethod);

        for ( int index=0; index<blockCount; ++index )
        {
            pResult->SetBlock
                (
                    index,
                    scratch->positions[index][0], scratch->positions[index][1],
                    scratch->blocks[index][0], scratch->blocks[index][1]
                );

			pResult->SetBlockOptions(index, scratch->priorities[index], (bool)scratch->ReduceBothAxes[index], (bool)scratch->ReduceByTwo[index]);
        }
    }
}
