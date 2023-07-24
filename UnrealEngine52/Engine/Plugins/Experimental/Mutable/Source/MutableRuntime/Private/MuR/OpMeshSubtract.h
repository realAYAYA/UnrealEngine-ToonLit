// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
    inline MeshPtr MeshSubtract( const Mesh* pA, const Mesh* pB )
	{
		// TODO: Optimise, generate optimised meshes

		MeshPtr pResult = pA->Clone();

		if ( pA->GetVertexCount()==0 || pB->GetVertexCount()==0 )
		{
			return pResult;
		}

		check( pResult->GetIndexBuffers().GetBufferCount()==1 );
		check( pResult->GetIndexBuffers().GetBufferChannelCount(0)==1 );
		check( pResult->GetFaceBuffers().GetBufferCount()==0 );

        UntypedMeshBufferIteratorConst itSource( pResult->GetIndexBuffers(), MBS_VERTEXINDEX );
        UntypedMeshBufferIterator itDest( pResult->GetIndexBuffers(), MBS_VERTEXINDEX );

		// For every face in A, see if it is also in B.
		Mesh::VERTEX_MATCH_MAP vertexMap;
		pB->GetVertexMap( *pA, vertexMap );
		int aFaceCount = pResult->GetFaceCount();
		int bFaceCount = pB->GetFaceCount();

        UntypedMeshBufferIteratorConst ito( pResult->GetIndexBuffers(), MBS_VERTEXINDEX );
		for ( int f=0; f<aFaceCount; ++f )
		{
			bool hasFace = false;
            vec3<uint32_t> ov;
            ov[0] = ito.GetAsUINT32(); ++ito;
            ov[1] = ito.GetAsUINT32(); ++ito;
            ov[2] = ito.GetAsUINT32(); ++ito;

            UntypedMeshBufferIteratorConst it( pB->GetIndexBuffers(), MBS_VERTEXINDEX );
            for ( int fb=0; !hasFace && fb<bFaceCount; fb++ )
			{
                vec3<uint32_t> v;
                v[0] = it.GetAsUINT32(); ++it;
                v[1] = it.GetAsUINT32(); ++it;
                v[2] = it.GetAsUINT32(); ++it;

				hasFace = true;
				for ( int vi=0; hasFace && vi<3; ++vi )
				{
					hasFace = vertexMap.Matches(v[vi],ov[0])
						 || vertexMap.Matches(v[vi],ov[1])
						 || vertexMap.Matches(v[vi],ov[2]);
				}
			}

			if ( !hasFace )
			{
				if ( itDest.ptr() != itSource.ptr() )
				{
					FMemory::Memcpy( itDest.ptr(), itSource.ptr(), itSource.GetElementSize()*3 );
				}

				itDest+=3;
			}

			itSource+=3;
		}

		std::size_t removedIndices = itSource - itDest;
		check( removedIndices%3==0 );

		pResult->GetFaceBuffers().SetElementCount( aFaceCount-(int)removedIndices/3 );
		pResult->GetIndexBuffers().SetElementCount( aFaceCount*3-(int)removedIndices );

		// TODO: Should redo/reorder the face buffer before SetElementCount since some deleted faces could be left and some remaining faces deleted.

		return pResult;
	}

}
