// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshSwitchPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshSwitch::Private::s_type =
			NODE_TYPE( "MeshSwitch", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshSwitch, EType::Switch, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshSwitch::GetInputCount() const
	{
		return 1 + m_pD->m_options.Num();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshSwitch::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		switch (i)
		{
		case 0:
			pResult = m_pD->m_pParameter.get();
			break;

		default:
			pResult = m_pD->m_options[i-1].get();
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSwitch::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pParameter = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		default:
			m_pD->m_options[i-1] = dynamic_cast<NodeMesh*>(pNode.get());
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeMeshSwitch::GetParameter() const
	{
		return m_pD->m_pParameter.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSwitch::SetParameter( NodeScalarPtr pNode )
	{
		m_pD->m_pParameter = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSwitch::SetOptionCount( int t )
	{
		m_pD->m_options.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMeshSwitch::GetOption( int t ) const
	{
		check( t>=0 && t<m_pD->m_options.Num() );
		return m_pD->m_options[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSwitch::SetOption( int t, NodeMeshPtr pNode )
	{
		check( t>=0 && t<m_pD->m_options.Num() );
		m_pD->m_options[t] = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshSwitch::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if (m_options.Num()>0 && m_options[0] )
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_options[0]->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}



}


