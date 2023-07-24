// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/Platform.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Reference version
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshTransform( const Mesh* pBase, const FMatrix44f& transform )
	{
		MeshPtr pDest = pBase->Clone();

        uint32_t vcount = pBase->GetVertexBuffers().GetElementCount();

        if ( !vcount )
		{
			return pDest;
		}

		FMatrix44f transformIT = transform.Inverse().GetTransposed();

        const FMeshBufferSet& MBSPriv = pDest->GetVertexBuffers();
        for ( int32 b=0; b<MBSPriv.m_buffers.Num(); ++b )
        {

            for ( int32 c=0; c<MBSPriv.m_buffers[b].m_channels.Num(); ++c )
            {
                MESH_BUFFER_SEMANTIC sem = MBSPriv.m_buffers[b].m_channels[c].m_semantic;
                int semIndex = MBSPriv.m_buffers[b].m_channels[c].m_semanticIndex;

                UntypedMeshBufferIterator it( pDest->GetVertexBuffers(), sem, semIndex );

                switch ( sem )
                {
                case MBS_POSITION:
                    for ( uint32_t v=0; v<vcount; ++v )
                    {
                        FVector4f value( 0.0f, 0.0f, 0.0f, 1.0f );
                        for( int i=0; i<it.GetComponents(); ++i )
                        {
                            ConvertData( i, &value[0], MBF_FLOAT32, it.ptr(), it.GetFormat() );
                        }

                        value = transform.TransformFVector4( value );

                        for( int i=0; i<it.GetComponents(); ++i )
                        {
                            ConvertData( i, it.ptr(), it.GetFormat(), &value[0], MBF_FLOAT32 );
                        }

                        ++it;
                    }
                    break;

                case MBS_NORMAL:
                case MBS_TANGENT:
                case MBS_BINORMAL:
                    for ( uint32_t v=0; v<vcount; ++v )
                    {
						FVector4f value( 0.0f, 0.0f, 0.0f, 1.0f );
                        for( int i=0; i<it.GetComponents(); ++i )
                        {
                            ConvertData( i, &value[0], MBF_FLOAT32, it.ptr(), it.GetFormat() );
                        }

                        value = transformIT.TransformFVector4(value);

                        for( int i=0; i<it.GetComponents(); ++i )
                        {
                            ConvertData( i, it.ptr(), it.GetFormat(), &value[0], MBF_FLOAT32 );
                        }

                        ++it;
                    }
                    break;

                default:
                    break;
                }
            }
        }


		return pDest;
	}

}

