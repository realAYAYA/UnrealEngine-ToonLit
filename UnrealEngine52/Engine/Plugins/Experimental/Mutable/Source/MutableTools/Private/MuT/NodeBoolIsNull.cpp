// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Node.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeBoolPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeBoolIsNull::Private::s_type =
			NODE_TYPE( "BoolIsNull", NodeBool::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeBoolIsNull, EType::IsNull, Node, Node::EType::Bool);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeBoolIsNull::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeBoolIsNull::GetInputNode( int ) const
	{
		return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeBoolIsNull::SetInputNode( int, NodePtr p )
	{
		m_pD->m_pSource = p;
	}

}


