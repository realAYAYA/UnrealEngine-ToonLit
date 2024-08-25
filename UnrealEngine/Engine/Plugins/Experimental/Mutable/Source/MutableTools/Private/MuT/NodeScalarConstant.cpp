// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarConstantPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeScalarConstant::Private::s_type =
			FNodeType( "ScalarConstant", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeScalarConstant, EType::Constant, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	float NodeScalarConstant::GetValue() const
	{
		return m_pD->m_value;
	}

	//---------------------------------------------------------------------------------------------
	void NodeScalarConstant::SetValue( float v )
	{
		m_pD->m_value = v;
	}


}

