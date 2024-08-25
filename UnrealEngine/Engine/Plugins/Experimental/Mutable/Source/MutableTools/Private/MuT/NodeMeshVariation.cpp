// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeMeshVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeMeshVariationPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    FNodeType NodeMeshVariation::Private::s_type =
        FNodeType( "MeshVariation", NodeMesh::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshVariation, EType::Variation, Node, Node::EType::Mesh)


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
