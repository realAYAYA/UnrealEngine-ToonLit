// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodePatchMesh.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodePatchMesh::Private::s_type =
			NODE_TYPE( "MeshPatch", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodePatchMesh, EType::PatchMesh, Node, Node::EType::PatchMesh);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodePatchMesh::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodePatchMesh::GetInputNode( int i ) const
	{
		Node* pResult = 0;

		if( i==0 )
		{
			pResult = m_pD->m_pRemove.get();
		}
		else if (i==1)
		{
			pResult = m_pD->m_pAdd.get();
		}
		else
		{
			check( false );
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodePatchMesh::SetInputNode( int i, NodePtr pNode )
	{
		if( i==0 )
		{
			m_pD->m_pRemove = dynamic_cast<NodeMesh*>( pNode.get() );
		}
		else if (i==1)
		{
			m_pD->m_pAdd = dynamic_cast<NodeMesh*>( pNode.get() );
		}
		else
		{
			check( false );
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMesh* NodePatchMesh::GetRemove() const
	{
        return m_pD->m_pRemove.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodePatchMesh::SetRemove( NodeMesh* pRemove )
	{
		m_pD->m_pRemove = pRemove;
	}


	//---------------------------------------------------------------------------------------------
    NodeMesh* NodePatchMesh::GetAdd() const
	{
        return m_pD->m_pAdd.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodePatchMesh::SetAdd( NodeMesh* pAdd )
	{
		m_pD->m_pAdd = pAdd;
	}



}


