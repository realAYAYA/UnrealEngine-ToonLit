// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/Platform.h"
#include "MuR/SparseIndexMap.h"

namespace mu
{
	
	//---------------------------------------------------------------------------------------------
	//! Optimized linear factor version for morphing 2 targets
	//---------------------------------------------------------------------------------------------
    inline MeshPtr MeshMorph2( const Mesh* pBase, const Mesh* pMin, const Mesh* pMax, const float factor )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshMorph2);

		if (!pBase) return nullptr;

        const auto ApplyMorph = []
            ( auto BaseIdIter, const TArray< UntypedMeshBufferIterator >& baseChannelsIters, const int32 baseSize,
              auto MorphIdIter, const TArray< UntypedMeshBufferIteratorConst >& morphChannelsIters, const int32 morphSize,
              const float factor )
        -> void
        {
            uint32 minBaseId = std::numeric_limits<uint32>::max();
            uint32 maxBaseId = 0;

			{
                auto limitsBaseIdIter = BaseIdIter;
				for ( int32 i = 0; i < baseSize; ++i, ++limitsBaseIdIter )
				{
					const uint32 id = limitsBaseIdIter.GetAsUINT32();
					minBaseId = FMath::Min( id, minBaseId );
					maxBaseId = FMath::Max( id, maxBaseId );
				}
			}

            SparseIndexMap IndexMap( minBaseId, maxBaseId );
            
            auto usedMorphIdIter = MorphIdIter;
            for ( uint32 i = 0; i < static_cast<uint32>(morphSize); ++i, ++usedMorphIdIter)
            {
                const uint32 morphId = (*usedMorphIdIter)[0];
                
                IndexMap.Insert( morphId, i );
            }

            for ( int32 v = 0; v < baseSize; ++v )
            {
                const uint32 baseId = (BaseIdIter + v).GetAsUINT32();
                const uint32 morphIdx = IndexMap.Find( baseId );

                if ( morphIdx == SparseIndexMap::NotFoundValue )
                {
                    continue;
                }

                const int32 m = static_cast<int32>( morphIdx );

                // Find consecutive run.
                auto RunBaseIter = BaseIdIter + v;
                auto RunMorphIter = MorphIdIter + m;

                int32 runSize = 0;
                for (  ; v + runSize < baseSize && m + runSize < morphSize && RunBaseIter.GetAsUINT32() == (*RunMorphIter)[0];
                        ++runSize, ++RunBaseIter, ++RunMorphIter );

                const size_t channelCount = morphChannelsIters.Num();
                for ( size_t c = 1; c < channelCount; ++c )
                {
                
                	if ( !(baseChannelsIters[c].ptr() && morphChannelsIters[c].ptr()) )
                	{
                		continue;
                	}
                
                    auto channelBaseIter = baseChannelsIters[c] + v;
                    auto channelMorphIter = morphChannelsIters[c] + m;
                   
                    const auto dstChannelFormat = baseChannelsIters[c].GetFormat();
                    const auto dstChannelComps = baseChannelsIters[c].GetComponents();

                    // Apply Morph to range found above.
                    for ( int32 r = 0; r < runSize; ++r, ++channelBaseIter, ++channelMorphIter )
                    {
                        const vec4<float> value = channelBaseIter.GetAsVec4f() + channelMorphIter.GetAsVec4f()*factor;

                        // TODO: Optimize this for the specific components.
                        // Max 4 components
                        for ( int32 comp = 0; comp < dstChannelComps && comp < 4; ++comp )
                        {
                            ConvertData( comp, channelBaseIter.ptr(), dstChannelFormat, &value, MBF_FLOAT32 );
                        }
                    }
                }

				v += FMath::Max(runSize - 1, 0);
            }
        };

		MeshPtr pDest = pBase->Clone();

		// Number of vertices to modify
		const uint32 minCount = pMin ? pMin->GetVertexBuffers().GetElementCount() : 0;
		const uint32 maxCount = pMax ? pMax->GetVertexBuffers().GetElementCount() : 0;
		const uint32 baseCount = pBase ? pBase->GetVertexBuffers().GetElementCount() : 0;
		const Mesh* refTarget = minCount > 0 ? pMin : pMax;

		if ( baseCount == 0 || (minCount + maxCount) == 0)
		{
			return pDest;
		}

		if (refTarget)
		{
			uint32 ccount = refTarget->GetVertexBuffers().GetBufferChannelCount(0);

			// Iterator of the vertex ids of the base vertices
			UntypedMeshBufferIteratorConst itBaseId(pBase->GetVertexBuffers(), MBS_VERTEXINDEX);

			TArray< UntypedMeshBufferIterator > itBaseChannels;
			itBaseChannels.SetNum(ccount);
			TArray< UntypedMeshBufferIteratorConst > itMinChannels;
			itMinChannels.SetNum(ccount);
			TArray< UntypedMeshBufferIteratorConst > itMaxChannels;
			itMaxChannels.SetNum(ccount);

			for (size_t c = 1; c < ccount; ++c)
			{
				const FMeshBufferSet& MBSPriv = refTarget->GetVertexBuffers();
				MESH_BUFFER_SEMANTIC sem = MBSPriv.m_buffers[0].m_channels[c].m_semantic;
				int semIndex = MBSPriv.m_buffers[0].m_channels[c].m_semanticIndex;
				
				itBaseChannels[c] = UntypedMeshBufferIterator(pDest->GetVertexBuffers(), sem, semIndex);
				if (minCount > 0)
				{
					itMinChannels[c] = UntypedMeshBufferIteratorConst(pMin->GetVertexBuffers(), sem, semIndex);
				}

				if (maxCount > 0)
				{
					itMaxChannels[c] = UntypedMeshBufferIteratorConst(pMax->GetVertexBuffers(), sem, semIndex);
				}
			}
			
			if (minCount > 0)
			{
				MeshBufferIteratorConst<MBF_UINT32, uint32, 1> itMinId(pMin->GetVertexBuffers(), MBS_VERTEXINDEX);
				ApplyMorph(itBaseId, itBaseChannels, baseCount, itMinId, itMinChannels, minCount, 1.0f - factor);
			}

			if (maxCount > 0)
			{
				MeshBufferIteratorConst<MBF_UINT32, uint32, 1> itMaxId(pMax->GetVertexBuffers(), MBS_VERTEXINDEX);
				ApplyMorph(itBaseId, itBaseChannels, baseCount, itMaxId, itMaxChannels, maxCount, factor);
			}
		}

        return pDest;
    }

	//---------------------------------------------------------------------------------------------
	//! \TODO Optimized linear factor version
	//---------------------------------------------------------------------------------------------
	inline MeshPtr MeshMorph(const Mesh* pBase, const Mesh* pMorph, float factor)
	{
		return MeshMorph2( pBase, nullptr, pMorph, factor );
	}

    //---------------------------------------------------------------------------------------------
    //! \TODO Optimized Factor-less version
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshMorph( const Mesh* pBase, const Mesh* pMorph )
    {
        // Trust the compiler to remove the factor
        return MeshMorph( pBase, pMorph, 1.0f );
    }
}
