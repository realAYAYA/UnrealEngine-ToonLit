// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeStringConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeStringConstantPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeStringConstant::Private::s_type =
			FNodeType( "StringConstant", NodeString::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeStringConstant, EType::Constant, Node, Node::EType::String)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeStringConstant::SetValue( const FString& v )
	{
		m_pD->m_value = v;
	}


}

