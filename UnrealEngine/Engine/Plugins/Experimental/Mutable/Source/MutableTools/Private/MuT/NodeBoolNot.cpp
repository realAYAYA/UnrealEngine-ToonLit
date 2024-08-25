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
	FNodeType NodeBoolNot::Private::s_type =
			FNodeType( "BoolParameter", NodeBool::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeBoolNot, EType::Not, Node, Node::EType::Bool);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	Ptr<NodeBool> NodeBoolNot::GetInput() const
	{
		return m_pD->m_pSource;
	}


	//---------------------------------------------------------------------------------------------
	void NodeBoolNot::SetInput(Ptr<NodeBool> p )
	{
		m_pD->m_pSource = p;
	}

}


