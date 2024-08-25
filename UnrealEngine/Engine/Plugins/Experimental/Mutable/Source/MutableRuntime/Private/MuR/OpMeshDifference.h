// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	//! Create a diff from the mesh vertices. The meshes must have the same amount of vertices.
	//! If the channel list is empty all the channels will be compared.
	//---------------------------------------------------------------------------------------------
    inline void MeshDifference( Mesh* Result, const Mesh* pBase, const Mesh* pTarget,
                                   int numChannels,
                                   const MESH_BUFFER_SEMANTIC* semantics,
                                   const int* semanticIndices,
                                   bool ignoreTexCoords, bool& bOutSuccess)

	{
		bOutSuccess = true;

        if (!pBase || !pTarget)
        {
			bOutSuccess = false;
            return;
        }

		bool bIsCorrect =
                pBase
                &&
				( pBase->GetVertexBuffers().GetElementCount()
						== pTarget->GetVertexBuffers().GetElementCount() )
				&&
                ( pBase->GetIndexCount() == pTarget->GetIndexCount() )
                &&
                ( pBase->GetVertexBuffers().GetElementCount()>0 )
                ;

		if (!bIsCorrect)
		{
			bOutSuccess = true; // Use the provided empty mesh as result.
			return ;
		}

		uint32 vcount = pBase->GetVertexBuffers().GetElementCount();		

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
		TArray< FVector4f > deltas;
		deltas.SetNum(vcount * numChannels);

		for ( int c=0; c<numChannels; ++c )
		{
			UntypedMeshBufferIteratorConst baseIt		
					( pBase->GetVertexBuffers(), semantics[c], semanticIndices[c] );

			UntypedMeshBufferIteratorConst targetIt		
					( pTarget->GetVertexBuffers(), semantics[c], semanticIndices[c] );

			for ( size_t v=0; v<vcount; ++v )
			{
				FVector4f base = baseIt.GetAsVec4f();
				FVector4f target = targetIt.GetAsVec4f();
				FVector4f delta = target-base;
				deltas[ v*numChannels+c ] = delta;		

				if (!delta.Equals(FVector4f(0,0,0,0),UE_SMALL_NUMBER))
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
			Result->GetVertexBuffers().SetElementCount( differentVertexCount );			
			Result->GetVertexBuffers().SetBufferCount( 1 );								

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

			Result->GetVertexBuffers().SetBuffer
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
			uint8_t* ResultData = Result->GetVertexBuffers().GetBufferData( 0 );
			for (size_t v=0; v<vcount; ++v)
			{
				if ( isVertexDifferent[v] )
				{
					// index
					*((uint32*)ResultData) = itBaseId.GetAsUINT32();			
					ResultData += 4;

					// all channels
					for ( int c=0; c<numChannels; ++c )
					{
						FMemory::Memcpy(ResultData, &deltas[v * numChannels + c], 16);
						ResultData += 4*4;											
					}
				}

				++itBaseId;
			}
		}


        // Rebuild surface data.
        // \todo: For now make single surfaced
        Result->m_surfaces.SetNum(0);
        Result->EnsureSurfaceData();
	}


}
