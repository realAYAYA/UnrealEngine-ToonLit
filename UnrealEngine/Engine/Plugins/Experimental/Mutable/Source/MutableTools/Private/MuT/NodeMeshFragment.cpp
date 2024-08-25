// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshFragment.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshFragmentPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeMeshFragment::Private::s_type =
			FNodeType( "MeshFragment", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshFragment, EType::Fragment, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshFragment::FRAGMENT_TYPE NodeMeshFragment::GetFragmentType() const
    {
        return m_pD->m_fragmentType;
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshFragment::SetFragmentType( FRAGMENT_TYPE type )
    {
        m_pD->m_fragmentType = type;
    }


    //---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshFragment::GetMesh() const
	{
		return m_pD->m_pMesh.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshFragment::SetMesh( NodeMeshPtr pNode )
	{
		m_pD->m_pMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
    int NodeMeshFragment::GetLayoutOrGroup() const
	{
        return m_pD->m_layoutOrGroup;
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshFragment::SetLayoutOrGroup( int l )
	{
        m_pD->m_layoutOrGroup = l;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshFragment::SetBlockCount( int t )
	{
		m_pD->m_blocks.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	int NodeMeshFragment::GetBlock( int t ) const
	{
		check( t>=0 && t<m_pD->m_blocks.Num() );
		return m_pD->m_blocks[t];
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshFragment::SetBlock( int t, int b )
	{
		check( t>=0 && t<m_pD->m_blocks.Num() );
		m_pD->m_blocks[t] = b;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshFragment::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if ( m_pMesh )
		{
			// TODO: Cut a fragment out of the layout.
			NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>( m_pMesh->GetBasePrivate() );
			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}



}


