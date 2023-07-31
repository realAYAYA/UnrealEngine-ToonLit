// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"

namespace mu
{

#pragma pack(push,1)
struct MORPH_VERTEX
{
    uint32_t index;
	vec3<float> pos;
	vec3<float> nor;
};
#pragma pack(pop)


	//---------------------------------------------------------------------------------------------
	//! Create a diff from the mesh vertices. The meshes must have the same amount of vertices.
	//! If the channel list is empty all the channels will be compared.
	//---------------------------------------------------------------------------------------------
    inline MeshPtr MeshDifference( const Mesh* pBase, const Mesh* pTarget,
                                   int numChannels,
                                   const MESH_BUFFER_SEMANTIC* semantics,
                                   const int* semanticIndices,
                                   bool ignoreTexCoords )

	{
        if (!pBase || !pTarget)
        {
            return nullptr;
        }

		bool correct =
                pBase
                &&
				( pBase->GetVertexBuffers().GetElementCount()
						== pTarget->GetVertexBuffers().GetElementCount() )
				&&
                ( pBase->GetIndexCount() == pTarget->GetIndexCount() )
                &&
                ( pBase->GetVertexBuffers().GetElementCount()>0 )
                ;

		MeshPtr pDest = new Mesh();				

		if (!correct)
            return pDest;

		uint32_t vcount = pBase->GetVertexBuffers().GetElementCount();		

		// If no channels were specified, get them all
		TArray<MESH_BUFFER_SEMANTIC> allSemantics;		
		TArray<int> allSemanticIndices;
		if ( !numChannels )
		{
			for ( int vb=0; vb<pBase->GetVertexBuffers().GetBufferCount(); ++vb )
			{
				for ( int c=0; c<pBase->GetVertexBuffers().GetBufferChannelCount(vb); ++c )
				{
					MESH_BUFFER_SEMANTIC sem = MBS_NONE;
					int semIndex = 0;
					pBase->GetVertexBuffers().GetChannel( vb, c,
															&sem, &semIndex,
															nullptr, nullptr, nullptr );			
					if ( sem != MBS_VERTEXINDEX &&
					     sem != MBS_BONEINDICES &&
					     sem != MBS_BONEWEIGHTS &&
					     sem != MBS_CHART &&
					     sem != MBS_LAYOUTBLOCK &&
					     sem != MBS_OTHER && 
					     ( !ignoreTexCoords || sem!=MBS_TEXCOORDS ) )
					{
						allSemantics.Add( sem );
						allSemanticIndices.Add( semIndex );
						++numChannels;
					}
				}
			}

			semantics = &allSemantics[0];
			semanticIndices = &allSemanticIndices[0];
		}


		// Make a delta of every vertex
		// TODO: Not always 4 components
		// TODO: Not always floats
		int differentVertexCount = 0;
		TArray<bool> isVertexDifferent;
		isVertexDifferent.SetNumZeroed(vcount);
		TArray< vec4<float> > deltas;
		deltas.SetNum(vcount * numChannels);

		for ( int c=0; c<numChannels; ++c )
		{
			UntypedMeshBufferIteratorConst baseIt		
					( pBase->GetVertexBuffers(), semantics[c], semanticIndices[c] );

			UntypedMeshBufferIteratorConst targetIt		
					( pTarget->GetVertexBuffers(), semantics[c], semanticIndices[c] );

			for ( size_t v=0; v<vcount; ++v )
			{
				vec4<float> base = baseIt.GetAsVec4f();			
				vec4<float> target = targetIt.GetAsVec4f();		
				vec4<float> delta = target-base;
				deltas[ v*numChannels+c ] = delta;		

				if (!delta.AlmostNull())
				{
					if ( !isVertexDifferent[v] )
					{
						++differentVertexCount;
					}

					isVertexDifferent[v] = true;
				}

				++baseIt;		
				++targetIt;		
			}
		}


		// Create the morph mesh
		{
			pDest->GetVertexBuffers().SetElementCount( differentVertexCount );			
			pDest->GetVertexBuffers().SetBufferCount( 1 );								

			TArray<MESH_BUFFER_SEMANTIC> semantic;
			TArray<int> semanticIndex;
			TArray<MESH_BUFFER_FORMAT> format;
			TArray<int> components;
			TArray<int> offsets;
			int offset = 0;										

			// Vertex index channel
			semantic.Add( MBS_VERTEXINDEX );					
			semanticIndex.Add( 0 );
			format.Add( MBF_UINT32 );
			components.Add( 1 );
			offsets.Add( offset );
			offset += 4;						

			for ( int c=0; c<numChannels; ++c )
			{
				semantic.Add( semantics[c] );
				semanticIndex.Add( semanticIndices[c] );
				format.Add( MBF_FLOAT32 );
				components.Add( 4 );
				offsets.Add( offset );
				offset += 16;				
			}

			pDest->GetVertexBuffers().SetBuffer
				(
					0,
					offset,
					numChannels+1,
					&semantic[0],
					&semanticIndex[0],
					&format[0],
					&components[0],
					&offsets[0]
				);

			// Source vertex index channel
			UntypedMeshBufferIteratorConst itBaseId( pBase->GetVertexBuffers(), MBS_VERTEXINDEX, 0 );
			check(itBaseId.GetElementSize());

			// Set the data
			uint8_t* pDestData = pDest->GetVertexBuffers().GetBufferData( 0 );
			for (size_t v=0; v<vcount; ++v)
			{
				if ( isVertexDifferent[v] )
				{
					// index
					*((uint32_t*)pDestData) = itBaseId.GetAsUINT32();			
					pDestData += 4;

					// all channels
					for ( int c=0; c<numChannels; ++c )
					{
						*((vec4<float>*)pDestData) = deltas[ v*numChannels+c ];		
						pDestData += 4*4;											
					}
				}

				++itBaseId;
			}
		}


        // Rebuild surface data.
        // \todo: For now make single surfaced
        pDest->m_surfaces.SetNum(0);
        pDest->EnsureSurfaceData();

		return pDest;
	}

	/*/
	{
		bool correct =
				( pBase->GetVertexBuffers().GetElementCount()
						== pTarget->GetVertexBuffers().GetElementCount() )
				&&
				( pBase->GetIndexCount() == pTarget->GetIndexCount()
				&&
				 pBase->GetVertexBuffers().GetElementCount()>0
				);

		MeshPtr pDest = new Mesh();

		if (correct)
		{
			// For now, just make a difference from the position and normal buffers.
			UINT32 vcount = pBase->GetVertexBuffers().GetElementCount();

			// Base mesh position
			int basePosElementSize = 0;
			const UINT8* pBasePosBuf = 0;
			GetMeshBuf( pBase, MBS_POSITION, MBF_FLOAT32, 3, pBasePosBuf, basePosElementSize );

			// Base mesh normal
			int baseNorElementSize = 0;
			const UINT8* pBaseNorBuf = 0;
			GetMeshBuf( pBase, MBS_NORMAL, MBF_FLOAT32, 3, pBaseNorBuf, baseNorElementSize );

			// Target mesh position
			int targetPosElementSize = 0;
			const UINT8* pTargetPosBuf = 0;
			GetMeshBuf( pTarget, MBS_POSITION, MBF_FLOAT32, 3, pTargetPosBuf, targetPosElementSize );

			// Target mesh normal
			int targetNorElementSize = 0;
			const UINT8* pTargetNorBuf = 0;
			GetMeshBuf( pTarget, MBS_NORMAL, MBF_FLOAT32, 3, pTargetNorBuf, targetNorElementSize );

			// Actual diff
			vector< MORPH_VERTEX > vertices;

			for ( UINT32 vi=0; vi<vcount; ++vi )
			{
				MORPH_VERTEX v;

				vec3<float> basePos = *reinterpret_cast< const vec3<float>* >( pBasePosBuf );
				vec3<float> targetPos = *reinterpret_cast< const vec3<float>* >( pTargetPosBuf );
				v.pos = targetPos - basePos;

				vec3<float> baseNor = *reinterpret_cast< const vec3<float>* >( pBaseNorBuf );
				vec3<float> targetNor = *reinterpret_cast< const vec3<float>* >( pTargetNorBuf );
				v.nor = targetNor - baseNor;

				if ( !( v.pos.AlmostNull() &&
						v.nor.AlmostNull() ) )
				{
					v.index = vi;
					vertices.push_back( v );
				}

				pBasePosBuf += basePosElementSize;
				pTargetPosBuf += targetPosElementSize;
				pBaseNorBuf += baseNorElementSize;
				pTargetNorBuf += targetNorElementSize;
			}

			// Create the morph mesh
			{
				int morphCount = (int)vertices.size();
				pDest->GetVertexBuffers().SetElementCount( morphCount );
				pDest->GetVertexBuffers().SetBufferCount( 1 );

				// Vertex indices
				{
					vector<MESH_BUFFER_SEMANTIC> semantic;
					semantic.push_back( MBS_VERTEXINDEX );
					semantic.push_back( MBS_POSITION );
					semantic.push_back( MBS_NORMAL );

					vector<int> semanticIndex;
					semanticIndex.push_back( 0 );
					semanticIndex.push_back( 0 );
					semanticIndex.push_back( 0 );

					vector<MESH_BUFFER_FORMAT> format;
					format.push_back( MBF_UINT32 );
					format.push_back( MBF_FLOAT32 );
					format.push_back( MBF_FLOAT32 );

					vector<int> components;
					components.push_back( 1 );
					components.push_back( 3 );
					components.push_back( 3 );

					vector<int> offset;
					offset.push_back( 0 );
					offset.push_back( 4 );
					offset.push_back( 16 );

					pDest->GetVertexBuffers().SetBuffer
						(
							0,
							28,
							3,
							&semantic[0],
							&semanticIndex[0],
							&format[0],
							&components[0],
							&offset[0]
						);

					UINT8* pDestData = pDest->GetVertexBuffers().GetBufferData( 0 );
					int size = morphCount*sizeof(MORPH_VERTEX);
					memcpy( pDestData, &vertices[0], size );
				}

			}
		}

		return pDest;
	}
*/
}
