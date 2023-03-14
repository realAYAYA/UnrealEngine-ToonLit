// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StaticArray.h"

#include "Templates/UniquePtr.h"

#include "Math/NumericLimits.h"

namespace mu
{

    /** 
	 * Very basic sparse index map where the i-th element contains the mapped index.
     * Only indices in the range [ MinIndex, MaxIndex ] can be accessed.
     *
     **/
    class SparseIndexMap
    {
    public: 
        static constexpr uint32 NotFoundValue = TNumericLimits<uint32>::Max();

    private:
        static constexpr uint32 Log2BlockSizeBytes = 12; 
        static constexpr uint32 Log2IndexSizeBytes = 2; 
        
		static_assert( (1 << Log2IndexSizeBytes) == sizeof(uint32) );
        static_assert( Log2BlockSizeBytes > Log2IndexSizeBytes );

        static constexpr uint32 Log2BlockElems = Log2BlockSizeBytes - Log2IndexSizeBytes;  
     
        const uint32 MinIndex;
        const uint32 MaxIndex;

        using BlockType = TStaticArray<uint32, 1 << Log2BlockElems>;
        
        TArray< TUniquePtr<BlockType> > Blocks;
        
    public:

        SparseIndexMap( const uint32 InMinIndex, const uint32 InMaxIndex )
            : MinIndex( InMinIndex )
            , MaxIndex( InMaxIndex )
        {
            check( InMinIndex <= InMaxIndex );

            const uint32 BlockCount = InMinIndex > InMaxIndex 
                                    ? 0 
                                    : ( (InMaxIndex - InMinIndex + 1) >> Log2BlockElems ) + 1;

            Blocks.SetNum( BlockCount ); 
        }


        bool Insert( const uint32 KeyIndex, const uint32 ValueIndex )
        {
            if ( !(KeyIndex >= MinIndex && KeyIndex <= MaxIndex) )
            {
                return false;
            }

            const uint32 MappedIndex = KeyIndex - MinIndex;

            TUniquePtr< BlockType >& BlockPtr = Blocks[MappedIndex >> Log2BlockElems];
            
            if ( !BlockPtr )
            {
                BlockPtr = MakeUnique<BlockType>(  
                            MakeUniformStaticArray<uint32_t, 1 << Log2BlockElems>( NotFoundValue ) );
            }

            (*BlockPtr)[ MappedIndex & ( ( 1 << Log2BlockElems ) - 1 ) ] = ValueIndex;

            return true;
        }

        uint32 Find( const uint32 KeyIndex ) const
        {
            if ( !(KeyIndex >= MinIndex && KeyIndex <= MaxIndex) )
            {
                return NotFoundValue;
            }

            const uint32 MappedIndex = KeyIndex - MinIndex;
            const uint32 BlockIndex = MappedIndex >> Log2BlockElems;

            check( BlockIndex < uint32(Blocks.Num()) );

            const TUniquePtr<BlockType>& BlockPtr = Blocks[ BlockIndex ];

            if ( !BlockPtr )
            {
                return NotFoundValue;
            }

            return (*BlockPtr)[ MappedIndex & ( ( 1 << Log2BlockElems ) - 1 ) ];
        }

    };


}
