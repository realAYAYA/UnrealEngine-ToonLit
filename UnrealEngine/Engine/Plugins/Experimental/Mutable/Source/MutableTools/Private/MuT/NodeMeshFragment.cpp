// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshFragment.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshFragmentPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshFragment::Private::s_type =
			NODE_TYPE( "MeshFragment", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshFragment, EType::Fragment, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshFragment::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshFragment::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );
        (void)i;

		Node* pResult = 0;

		pResult = m_pD->m_pMesh.get();

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshFragment::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pMesh = dynamic_cast<NodeMesh*>(pNode.get());
			break;

		default:
			break;
		}
	}


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

			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_pMesh->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}



}


