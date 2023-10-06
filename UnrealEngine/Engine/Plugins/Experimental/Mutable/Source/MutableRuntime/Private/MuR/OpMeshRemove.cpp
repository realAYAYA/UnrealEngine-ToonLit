// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/OpMeshRemove.h"

#include "MuR/MeshPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/Platform.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    struct ID_INTERVAL
    {
        int32 idStart;
        int32 idPosition;
        int32 size;
    };


    void ExtractVertexIndexIntervals( TArray<ID_INTERVAL>& intervals, const Mesh* pSource )
    {
        MeshBufferIteratorConst<MBF_UINT32,uint32,1> itVI( pSource->GetVertexBuffers(), MBS_VERTEXINDEX );
        ID_INTERVAL current;
		current.idStart = -1;
		current.idPosition = 0;
		current.size = 0;
		for ( int sv=0; sv<pSource->GetVertexBuffers().GetElementCount(); ++sv )
        {
            int32 id = (*itVI)[0];
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


    int FindPositionInIntervals( const TArray<ID_INTERVAL>& intervals, int32 id )
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


	void MeshRemoveVerticesWithMap( Mesh* Result, uint8* removedVertices, int32 resultVertexCount)
	{
		int32 firstFreeVertex = 0;

        // Rebuild index buffers

        // Map from source vertex index, to new vertex index for used vertices.
        // These are indices as in the index buffer, not the absoulte vertex index as in the
        // vertexbuffer MBS_VERTEXINDEX buffers.
		TArray<int32> usedVertices;
		usedVertices.Init( -1, Result->GetVertexCount() );
        {
            SIZE_T removedIndices = 0;

            if ( Result->GetIndexBuffers().GetElementSize(0)==4 )
            {
                MeshBufferIteratorConst<MBF_UINT32,uint32,1> itSource( Result->GetIndexBuffers(), MBS_VERTEXINDEX );
                MeshBufferIterator<MBF_UINT32,uint32,1> itDest( Result->GetIndexBuffers(), MBS_VERTEXINDEX );

                int32 indexCount = Result->GetIndexCount();
                for ( int32 f=0; f<indexCount/3; ++f )
                {
                    uint32 sourceIndices[3];
                    sourceIndices[0] = itSource[0][0];
                    sourceIndices[1] = itSource[1][0];
                    sourceIndices[2] = itSource[2][0];

                    bool faceRemoved =  ( removedVertices[ sourceIndices[0] ]
                            + removedVertices[ sourceIndices[1] ]
                            + removedVertices[ sourceIndices[2] ]
                            ) > 0;

                    if ( !faceRemoved )
                    {
                        for (int32 i=0;i<3;++i)
                        {
                            uint32 sourceIndex = sourceIndices[i];

                            if ( usedVertices[ sourceIndex ] < 0 )
                            {
                                usedVertices[ sourceIndex ] = firstFreeVertex;
                                firstFreeVertex++;
                            }

                            uint32 destIndex = usedVertices[ sourceIndex ];
                            *(uint32*)itDest.ptr() = destIndex;

                            itDest++;
                        }
                    }

                    itSource+=3;
                }

                removedIndices = itSource - itDest;
            }

            else if ( Result->GetIndexBuffers().GetElementSize(0)==2 )
            {
                MeshBufferIteratorConst<MBF_UINT16,uint16,1> itSource( Result->GetIndexBuffers(), MBS_VERTEXINDEX );
                MeshBufferIterator<MBF_UINT16,uint16,1> itDest( Result->GetIndexBuffers(), MBS_VERTEXINDEX );

                int32 indexCount = Result->GetIndexCount();
                for ( int32 f=0; f<indexCount/3; ++f )
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

            int32 faceCount = Result->GetFaceCount();
            Result->GetFaceBuffers().SetElementCount( faceCount-(int)removedIndices/3 );
            Result->GetIndexBuffers().SetElementCount( faceCount*3-(int)removedIndices );
        }

        // Rebuild the vertex buffers
        for ( int32 b=0; b<Result->GetVertexBuffers().GetBufferCount(); ++b )
        {
            int32 elemSize = Result->GetVertexBuffers().GetElementSize( b );
            const uint8* pSourceData = Result->GetVertexBuffers().GetBufferData( b );
            uint8* pData = Result->GetVertexBuffers().GetBufferData( b );
            for ( int32 v=0; v<Result->GetVertexCount(); ++v )
            {
                int32 span = 0;
                for ( int32 s=0; v+s<Result->GetVertexCount(); ++s )
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
                    FMemory::Memmove( pData+elemSize*usedVertices[v], pSourceData+elemSize*v, elemSize*span );

                    v += span-1;
                }
            }
        }
        Result->GetVertexBuffers().SetElementCount( firstFreeVertex );

        // Rebuild surface data.
        // \todo: For now make single surfaced
        Result->m_surfaces.Empty();
        Result->EnsureSurfaceData();
    }


	void MeshRemoveMask(Mesh* Result, const Mesh* pSource, const Mesh* pMask, bool& bOutSuccess)
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshRemoveMask);
		bOutSuccess = true;

        if (!pMask->GetVertexCount() || !pSource->GetVertexCount() || !pSource->GetIndexCount())
        {
			bOutSuccess = false;
            return;
        }

		Result->CopyFrom(*pSource);

        // TODO
        Result->SetFaceGroupCount( 0 );

        MeshBufferIteratorConst<MBF_UINT32,uint32,1> itMaskVI( pMask->GetVertexBuffers(), MBS_VERTEXINDEX );

        int32 firstFreeVertex = 0;

        // For each source vertex, true if it is removed.
        int32 resultVertexCount = Result->GetVertexCount();
		uint8* removedVertices = reinterpret_cast<uint8*>(FMemory::Malloc(resultVertexCount, 16));
		FMemory::Memzero(removedVertices,resultVertexCount);
        {
			TArray<ID_INTERVAL> intervals;
            ExtractVertexIndexIntervals(intervals, Result);

            for ( int32 mv=0; mv<pMask->GetVertexBuffers().GetElementCount(); ++mv )
            {
                uint32 maskVertexId = (*itMaskVI)[0];
                ++itMaskVI;

                int32 indexInSource = FindPositionInIntervals(intervals, maskVertexId);
                if (indexInSource>=0)
                {
                    removedVertices[indexInSource] = 1;
                }
            }
        }

		MeshRemoveVerticesWithMap(Result, removedVertices, resultVertexCount);

		FMemory::Free(removedVertices);
	}

}
