// Copyright Epic Games, Inc. All Rights Reserved.


#include "Misc/AssertionMacros.h"
#include "MuT/Node.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeBoolPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeBoolAnd::Private::s_type =
			NODE_TYPE( "BoolParameter", NodeBool::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeBoolAnd, EType::And, Node, Node::EType::Bool);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeBoolAnd::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeBoolAnd::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );
		if ( i == 0)
		{
			return m_pD->m_pA.get();
		}
		else
		{
			return m_pD->m_pB.get();
		}
	}


	//---------------------------------------------------------------------------------------------
	void NodeBoolAnd::SetInputNode( int i, NodePtr p )
	{
		check( i>=0 && i<GetInputCount() );
		if ( i == 0)
		{
			m_pD->m_pA = dynamic_cast<NodeBool*>( p.get() );
		}
		else
		{
			m_pD->m_pB = dynamic_cast<NodeBool*>( p.get() );
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeBoolPtr NodeBoolAnd::GetA() const
	{
		return m_pD->m_pA;
	}


	//---------------------------------------------------------------------------------------------
	void NodeBoolAnd::SetA( NodeBoolPtr p )
	{
		m_pD->m_pA = p;
	}


	//---------------------------------------------------------------------------------------------
	NodeBoolPtr NodeBoolAnd::GetB() const
	{
		return m_pD->m_pB;
	}


	//---------------------------------------------------------------------------------------------
	void NodeBoolAnd::SetB( NodeBoolPtr p )
	{
		m_pD->m_pB = p;
	}

}


