// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/Platform.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshRemapIndices( const Mesh* pBase, const Mesh* pReference )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshRemapIndices);

        if (!pBase) return nullptr;

        MeshPtr pDest = pBase->Clone();

        if (!pReference) return pDest;

        // Number of vertices to modify
        uint32_t vcountReference = pReference->GetVertexBuffers().GetElementCount();
        uint32_t vcountBase = pBase->GetVertexBuffers().GetElementCount();
        if ( !vcountReference || !vcountBase )
        {
            return pDest;
        }

        // Iterator of the vertex ids of the base vertices
        UntypedMeshBufferIteratorConst itReferenceId( pReference->GetVertexBuffers(), MBS_VERTEXINDEX );

        FMeshBufferSet& verts = pDest->m_VertexBuffers;
        for ( auto& buf: verts.m_buffers )
        {
            for ( auto& chan: buf.m_channels )
            {
                if (chan.m_semantic!=MBS_VERTEXINDEX)
                    continue;
                
                auto pData = buf.m_data.GetData() + chan.m_offset;

                switch( chan.m_format )
                {
                case MBF_INT32:
                case MBF_UINT32:
                    for ( uint32_t v = 0; v < vcountBase;  ++v)
                    {
                        uint32_t* pIndex = (uint32_t*)pData;
                        uint32_t oldIndex = *pIndex;
                        check( oldIndex < vcountReference );
                        uint32_t newIndex = (itReferenceId+oldIndex).GetAsUINT32();
                        *pIndex = newIndex;
                        pData += buf.m_elementSize;
                    }
                    break;

                case MBF_INT16:
                case MBF_UINT16:
                    for ( uint32_t v = 0; v < vcountBase;  ++v)
                    {
                        uint16* pIndex = (uint16*)pData;
                        uint16 oldIndex = *pIndex;
                        check( *pIndex < vcountReference );
                        uint32_t newIndex = ( itReferenceId + oldIndex ).GetAsUINT32();
                        *pIndex = uint16(newIndex);
                        pData += buf.m_elementSize;
                    }
                    break;

                default:
                    // Not implemented
                    check(false);
                    break;
                }
            }
        }

        return pDest;
    }

}
