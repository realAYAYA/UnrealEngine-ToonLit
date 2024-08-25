// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshClipWithMesh.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshClipWithMeshPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeMeshClipWithMesh::Private::s_type =
            FNodeType( "MeshClipMorphPlane", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshClipWithMesh, EType::ClipWithMesh, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshClipWithMesh::GetSource() const
	{
		return m_pD->m_pSource;
	}


    //---------------------------------------------------------------------------------------------
    void NodeMeshClipWithMesh::SetSource(NodeMesh* p)
    {
        m_pD->m_pSource = p;
    }


	//---------------------------------------------------------------------------------------------
	void NodeMeshClipWithMesh::SetClipMesh(NodeMesh* p)
	{
		m_pD->m_pClipMesh = p;
	}


	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeMeshClipWithMesh::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if ( m_pSource )
		{
			NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>( m_pSource->GetBasePrivate() );
			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshClipWithMesh::AddTag(const char* tagName)
	{
		m_pD->m_tags.Add(tagName);
	}
}
