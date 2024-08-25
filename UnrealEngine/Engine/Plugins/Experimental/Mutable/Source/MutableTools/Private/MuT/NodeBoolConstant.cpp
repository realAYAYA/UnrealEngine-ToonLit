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
	FNodeType NodeBoolConstant::Private::s_type =
			FNodeType( "BoolConstant", NodeBool::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeBoolConstant, EType::Constant, Node, Node::EType::Bool);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	bool NodeBoolConstant::GetValue() const
	{
		return m_pD->m_value;
	}

	//---------------------------------------------------------------------------------------------
	void NodeBoolConstant::SetValue( bool v )
	{
		m_pD->m_value = v;
	}


}
