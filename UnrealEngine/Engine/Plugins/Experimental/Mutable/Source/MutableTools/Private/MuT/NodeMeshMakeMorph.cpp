// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshMakeMorph.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshMakeMorphPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeMeshMakeMorph::Private::s_type =
			NODE_TYPE( "MeshMakeMorph", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshMakeMorph, EType::MakeMorph, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeMeshMakeMorph::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeMeshMakeMorph::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());
		return i == 0 ? m_pD->m_pBase.get() : m_pD->m_pTarget.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshMakeMorph::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());
		if (i==0)
		{
			m_pD->m_pBase = dynamic_cast<NodeMesh*>( pNode.get() );
		}
		else
		{
			m_pD->m_pTarget = dynamic_cast<NodeMesh*>( pNode.get() );
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshMakeMorph::GetBase() const
	{
		return m_pD->m_pBase;
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshMakeMorph::SetBase( NodeMesh* p )
	{
		m_pD->m_pBase = p;
	}


	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshMakeMorph::GetTarget() const
	{
		return m_pD->m_pTarget;
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshMakeMorph::SetTarget( NodeMesh* p )
	{
		m_pD->m_pTarget = p;
	}


	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeMeshMakeMorph::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		// TODO: Substract layouts too? Usually they are ignored.
		if ( m_pBase )
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_pBase->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}
