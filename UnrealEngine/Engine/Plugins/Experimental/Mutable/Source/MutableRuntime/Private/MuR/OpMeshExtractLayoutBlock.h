// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"


namespace mu
{


    //---------------------------------------------------------------------------------------------
    inline void MeshExtractFromVertices( const Mesh* pSource,
                                         Mesh* pResult,
                                         const TArray<int>& oldToNew,
                                         const TArray<int>& newToOld )
    {
        int resultVertices = newToOld.Num();

        // Assemble the new vertex buffer
        pResult->GetVertexBuffers().SetElementCount( resultVertices );
        for ( int b=0; b<pResult->GetVertexBuffers().GetBufferCount(); ++b )
        {
            const uint8_t* pSourceData = pSource->GetVertexBuffers().GetBufferData(b);
            uint8_t* pDest = pResult->GetVertexBuffers().GetBufferData(b);
            int size = pResult->GetVertexBuffers().GetElementSize(b);
            for ( int v=0; v<resultVertices; ++v )
            {
                int i = newToOld[v];
                memcpy( pDest, pSourceData + size*i, size );
                pDest+=size;
            }
        }

        // Assemble the new index buffers
		TArray<bool> usedSourceFaces;
		usedSourceFaces.SetNumZeroed(pSource->GetFaceCount());
        UntypedMeshBufferIteratorConst itIndex( pSource->GetIndexBuffers(), MBS_VERTEXINDEX );
        UntypedMeshBufferIterator itResultIndex( pResult->GetIndexBuffers(), MBS_VERTEXINDEX );
        int indexCount = 0;
        if ( itIndex.GetFormat()==MBF_UINT32 )
        {
            const uint32_t* pIndices = reinterpret_cast<const uint32_t*>( itIndex.ptr() );
            uint32_t* pDestIndices = reinterpret_cast<uint32_t*>( itResultIndex.ptr() );
            for ( int i=0; i<pSource->GetIndexCount()/3; ++i )
            {
                if ( oldToNew[ pIndices[i*3+0] ]>=0
                     &&
                     oldToNew[ pIndices[i*3+1] ]>=0
                     &&
                     oldToNew[ pIndices[i*3+2] ]>=0 )
                {
                    usedSourceFaces[i] = true;

                    // Clamp in case triangles go across blocks
                    pDestIndices[ indexCount++ ] = FMath::Max( 0, oldToNew[ pIndices[i*3+0] ] );
                    pDestIndices[ indexCount++ ] = FMath::Max( 0, oldToNew[ pIndices[i*3+1] ] );
                    pDestIndices[ indexCount++ ] = FMath::Max( 0, oldToNew[ pIndices[i*3+2] ] );
                }
            }
        }
        else if ( itIndex.GetFormat()==MBF_UINT16 )
        {
            const uint16* pIndices = reinterpret_cast<const uint16*>( itIndex.ptr() );
            uint16* pDestIndices = reinterpret_cast<uint16*>( itResultIndex.ptr() );
            for ( int i=0; i<pSource->GetIndexCount()/3; ++i )
            {
                if ( oldToNew[ pIndices[i*3+0] ]>=0
                     &&
                     oldToNew[ pIndices[i*3+1] ]>=0
                     &&
                     oldToNew[ pIndices[i*3+2] ]>=0 )
                {
                    usedSourceFaces[i] = true;

                    // Clamp in case triangles go across blocks
                    pDestIndices[ indexCount++ ] = (uint16)FMath::Max( 0, oldToNew[ pIndices[i*3+0] ] );
                    pDestIndices[ indexCount++ ] = (uint16)FMath::Max( 0, oldToNew[ pIndices[i*3+1] ] );
                    pDestIndices[ indexCount++ ] = (uint16)FMath::Max( 0, oldToNew[ pIndices[i*3+2] ] );
                }
            }
        }
        else
        {
            check( false );
        }
        pResult->GetIndexBuffers().SetElementCount( indexCount );


        // Face buffers
        {
            int faceCount = 0;
            for ( int i=0; i<pSource->GetFaceCount(); ++i )
            {
                if ( usedSourceFaces[i] )
                {
                    for ( int b=0; b<pResult->GetFaceBuffers().GetBufferCount(); ++b )
                    {
                        const uint8_t* pSourceData = pSource->GetFaceBuffers().GetBufferData(b);
                        uint8_t* pDest = pResult->GetFaceBuffers().GetBufferData(b);
                        int size = pResult->GetFaceBuffers().GetElementSize(b);
                        memcpy( pDest + size*faceCount, pSourceData + size*i, size );
                    }

                    faceCount++;
                }
            }
            check( faceCount == indexCount/3 );
            pResult->GetFaceBuffers().SetElementCount( faceCount );
        }
    }


	//---------------------------------------------------------------------------------------------
    inline void MeshExtractLayoutBlock(Mesh* Result, const Mesh* pSource,
                                           uint32 layout,
                                           uint16 blockCount,
                                           const uint32* pExtractBlocks, bool& bOutSuccess)
	{
		check(pSource);
		bOutSuccess = true;
		
		// TODO: Optimise
		Result->CopyFrom(*pSource);

		UntypedMeshBufferIteratorConst itBlocks( pSource->GetVertexBuffers(),
												 MBS_LAYOUTBLOCK, layout );

        if (itBlocks.GetFormat()!=MBF_NONE)
        {
            int resultVertices = 0;
			TArray<int> oldToNew;
			oldToNew.Init(-1,pSource->GetVertexCount());
			TArray<int> newToOld;
            newToOld.Reserve( pSource->GetVertexCount() );

            if ( itBlocks.GetFormat()==MBF_UINT16 )
            {
                const uint16* pBlocks = reinterpret_cast<const uint16*>( itBlocks.ptr() );
                for ( int i=0; i<pSource->GetVertexCount(); ++i )
                {
                    uint32_t vertexBlock = pBlocks[i];

                    bool found = false;
                    for ( int j=0; j<blockCount; ++j)
                    {
                        if (vertexBlock==pExtractBlocks[j])
                        {
                            found = true;
                            break;
                        }
                    }

                    if ( found )
                    {
                        oldToNew[i] = resultVertices++;
                        newToOld.Add( i );
                    }
                }
            }
            else
            {
                check( false );
            }

            MeshExtractFromVertices(pSource, Result, oldToNew, newToOld);
        }

        Result->m_surfaces.Empty();
        Result->EnsureSurfaceData();
	}

}
