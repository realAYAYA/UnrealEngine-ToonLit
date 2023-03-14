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
	NODE_TYPE NodeBoolNot::Private::s_type =
			NODE_TYPE( "BoolParameter", NodeBool::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeBoolNot, EType::Not, Node, Node::EType::Bool);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeBoolNot::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeBoolNot::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );
        (void)i;
        return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeBoolNot::SetInputNode( int i, NodePtr p)
	{
		check( i>=0 && i<GetInputCount() );
        (void)i;
        m_pD->m_pSource = dynamic_cast<NodeBool*>( p.get() );
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeBoolPtr NodeBoolNot::GetInput() const
	{
		return m_pD->m_pSource;
	}


	//---------------------------------------------------------------------------------------------
	void NodeBoolNot::SetInput( NodeBoolPtr p )
	{
		m_pD->m_pSource = p;
	}

}


