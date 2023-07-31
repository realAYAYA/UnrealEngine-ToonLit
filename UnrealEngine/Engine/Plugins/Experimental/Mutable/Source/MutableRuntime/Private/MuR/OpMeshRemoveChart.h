// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    struct ID_INTERVAL
    {
        int idStart;
        int idPosition;
        int size;
    };


    inline void ExtractVertexIndexIntervals( TArray<ID_INTERVAL>& intervals, const Mesh* pSource )
    {
        MeshBufferIteratorConst<MBF_UINT32,uint32_t,1> itVI( pSource->GetVertexBuffers(), MBS_VERTEXINDEX );
        ID_INTERVAL current;
		current.idStart = -1;
		current.idPosition = 0;
		current.size = 0;
		for ( int sv=0; sv<pSource->GetVertexBuffers().GetElementCount(); ++sv )
        {
            int32_t id = (*itVI)[0];
            ++itVI;

            if (current.idStart<0)
            {
                current.idStart = id;
                current.idPosition = sv;
                current.size = 1;
            }
            else
            {
                if (id==current.idStart+current.size)
                {
                    ++current.size;
                }
                else
                {
                    intervals.Add(current);
                    current.idStart = id;
                    current.idPosition = sv;
                    current.size = 1;
                }
            }
        }

        if (current.idStart>=0)
        {
            intervals.Add(current);
        }
    }


    inline int FindPositionInIntervals( const TArray<ID_INTERVAL>& intervals, int id )
    {
        for( const auto& interval: intervals )
        {
            int deltaId = id - interval.idStart;
            if (deltaId>=0 && deltaId<interval.size)
            {
                return interval.idPosition+deltaId;
            }
        }
        return -1;
    }


    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshRemoveMask( const Mesh* pSource, const Mesh* pMask )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshRemoveMask);

        MeshPtr pResult = pSource->Clone();

        if (!pMask->GetVertexCount() || !pResult->GetVertexCount() ||
			!pResult->GetIndexCount() )
        {
            return pResult;
        }

        // TODO
        pResult->SetFaceGroupCount( 0 );

        MeshBufferIteratorConst<MBF_UINT32,uint32_t,1> itMaskVI( pMask->GetVertexBuffers(), MBS_VERTEXINDEX );

        int firstFreeVertex = 0;

        // For each source vertex, true if it is removed.
        int resultVertexCount = pResult->GetVertexCount();
        uint8_t* removedVertices = (uint8_t*)mutable_malloc_aligned( resultVertexCount, 16 );
		FMemory::Memzero(removedVertices,resultVertexCount);
        {
			TArray<ID_INTERVAL> intervals;
            ExtractVertexIndexIntervals( intervals, pResult.get() );

            for ( int mv=0; mv<pMask->GetVertexBuffers().GetElementCount(); ++mv )
            {
                uint32_t maskVertexId = (*itMaskVI)[0];
                ++itMaskVI;

                int indexInSource = FindPositionInIntervals(intervals, maskVertexId);
                if (indexInSource>=0)
                {
                    removedVertices[indexInSource] = 1;
                }
            }
        }

        // Rebuild index buffers

        // Map from source vertex index, to new vertex index for used vertices.
        // These are indices as in the index buffer, not the absoulte vertex index as in the
        // vertexbuffer MBS_VERTEXINDEX buffers.
		TArray<int> usedVertices;
		usedVertices.Init( -1, pResult->GetVertexCount() );
        {
            size_t removedIndices = 0;

            if ( pResult->GetIndexBuffers().GetElementSize(0)==4 )
            {
                MeshBufferIteratorConst<MBF_UINT32,uint32_t,1> itSource( pResult->GetIndexBuffers(), MBS_VERTEXINDEX );
                MeshBufferIterator<MBF_UINT32,uint32_t,1> itDest( pResult->GetIndexBuffers(), MBS_VERTEXINDEX );

                int indexCount = pResult->GetIndexCount();
                for ( int f=0; f<indexCount/3; ++f )
                {
                    uint32_t sourceIndices[3];
                    sourceIndices[0] = itSource[0][0];
                    sourceIndices[1] = itSource[1][0];
                    sourceIndices[2] = itSource[2][0];

                    bool faceRemoved =  ( removedVertices[ sourceIndices[0] ]
                            + removedVertices[ sourceIndices[1] ]
                            + removedVertices[ sourceIndices[2] ]
                            ) > 0;

                    if ( !faceRemoved )
                    {
                        for (int i=0;i<3;++i)
                        {
                            uint32_t sourceIndex = sourceIndices[i];

                            if ( usedVertices[ sourceIndex ] < 0 )
                            {
                                usedVertices[ sourceIndex ] = firstFreeVertex;
                                firstFreeVertex++;
                            }

                            uint32_t destIndex = usedVertices[ sourceIndex ];
                            *(uint32_t*)itDest.ptr() = destIndex;

                            itDest++;
                        }
                    }

                    itSource+=3;
                }

                removedIndices = itSource - itDest;
            }

            else if ( pResult->GetIndexBuffers().GetElementSize(0)==2 )
            {
                MeshBufferIteratorConst<MBF_UINT16,uint16,1> itSource( pResult->GetIndexBuffers(), MBS_VERTEXINDEX );
                MeshBufferIterator<MBF_UINT16,uint16,1> itDest( pResult->GetIndexBuffers(), MBS_VERTEXINDEX );

                int indexCount = pResult->GetIndexCount();
                for ( int f=0; f<indexCount/3; ++f )
                {
                    uint16 sourceIndices[3];
                    sourceIndices[0] = itSource[0][0];
                    sourceIndices[1] = itSource[1][0];
                    sourceIndices[2] = itSource[2][0];

                    bool faceRemoved =  ( removedVertices[ sourceIndices[0] ]
                            + removedVertices[ sourceIndices[1] ]
                            + removedVertices[ sourceIndices[2] ]
                            ) > 0;

                    if ( !faceRemoved )
                    {
                        for (int i=0;i<3;++i)
                        {
                            uint16 sourceIndex = sourceIndices[i];

                            if ( usedVertices[ sourceIndex ] < 0 )
                            {
                                usedVertices[ sourceIndex ] = firstFreeVertex;
                                firstFreeVertex++;
                            }

                            uint16 destIndex = (uint16)usedVertices[ sourceIndex ];
                            *(uint16*)itDest.ptr() = destIndex;

                            itDest++;
                        }
                    }

                    itSource+=3;
                }

                removedIndices = itSource - itDest;
            }

            else
            {
                // Case not implemented
                check( false );
            }

            check( removedIndices%3==0 );

            int faceCount = pResult->GetFaceCount();
            pResult->GetFaceBuffers().SetElementCount( faceCount-(int)removedIndices/3 );
            pResult->GetIndexBuffers().SetElementCount( faceCount*3-(int)removedIndices );
        }

        mutable_free_aligned( removedVertices, resultVertexCount );

        // Rebuild the vertex buffers
        for ( int b=0; b<pResult->GetVertexBuffers().GetBufferCount(); ++b )
        {
            int elemSize = pResult->GetVertexBuffers().GetElementSize( b );
            const uint8_t* pSourceData = pSource->GetVertexBuffers().GetBufferData( b );
            uint8_t* pData = pResult->GetVertexBuffers().GetBufferData( b );
            for ( int v=0; v<pResult->GetVertexCount(); ++v )
            {
                int span = 0;
                for ( int s=0; v+s<pResult->GetVertexCount(); ++s )
                {
                    if ( usedVertices[v+s]>=0 )
                    {
                        if (span==0)
                        {
                            ++span;
                        }
                        else
                        {
                            if ( usedVertices[v+s] == usedVertices[v+s-1]+1 )
                            {
                                ++span;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                if (span>0)
                {
                    memcpy( pData+elemSize*usedVertices[v], pSourceData+elemSize*v, elemSize*span );

                    v += span-1;
                }
            }
        }
        pResult->GetVertexBuffers().SetElementCount( firstFreeVertex );

        // Rebuild surface data.
        // \todo: For now make single surfaced
        pResult->m_surfaces.Empty();
        pResult->EnsureSurfaceData();

        return pResult;
    }

}
