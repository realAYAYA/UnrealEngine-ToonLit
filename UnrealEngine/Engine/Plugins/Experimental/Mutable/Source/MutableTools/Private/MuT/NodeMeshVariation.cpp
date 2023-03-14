// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeMeshVariationPrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    NODE_TYPE NodeMeshVariation::Private::s_type =
        NODE_TYPE( "MeshVariation", NodeMesh::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshVariation, EType::Variation, Node, Node::EType::Mesh)


    //---------------------------------------------------------------------------------------------
    int NodeMeshVariation::GetInputCount() const { return 1 + m_pD->m_variations.Num(); }


    //---------------------------------------------------------------------------------------------
    Node* NodeMeshVariation::GetInputNode( int i ) const
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            return m_pD->m_defaultMesh.get();
        }
        i -= 1;

        if ( i < m_pD->m_variations.Num() )
        {
            return m_pD->m_variations[i].m_mesh.get();
        }
        i -= m_pD->m_variations.Num();

        return nullptr;
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshVariation::SetInputNode( int i, NodePtr pNode )
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            m_pD->m_defaultMesh = dynamic_cast<NodeMesh*>( pNode.get() );
            return;
        }

        i -= 1;
        if ( i < m_pD->m_variations.Num() )
        {

            m_pD->m_variations[i].m_mesh = dynamic_cast<NodeMesh*>( pNode.get() );
            return;
        }
        i -= m_pD->m_variations.Num();
    }


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodeMeshVariation::SetDefaultMesh( NodeMesh* p ) 
	{ 
		m_pD->m_defaultMesh = p; 
	}


    //---------------------------------------------------------------------------------------------
    int NodeMeshVariation::GetVariationCount() const 
	{ 
		return  m_pD->m_variations.Num(); 
	}


    //---------------------------------------------------------------------------------------------
    void NodeMeshVariation::SetVariationCount( int num )
    {
        check( num >= 0 );
        m_pD->m_variations.SetNum( num );
    }

    //---------------------------------------------------------------------------------------------
    void NodeMeshVariation::SetVariationTag( int index, const char* strTag )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );
        check( strTag );

        if ( strTag )
        {
            m_pD->m_variations[index].m_tag = strTag;
        }
        else
        {
            m_pD->m_variations[index].m_tag = "";
        }
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshVariation::SetVariationMesh( int index, NodeMesh* pNode )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );

        m_pD->m_variations[index].m_mesh = pNode;
    }


} // namespace mu
