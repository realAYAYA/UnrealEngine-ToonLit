// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarSwitchPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeScalarSwitch::Private::s_type =
            FNodeType( "ScalarSwitch", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeScalarSwitch, EType::Switch, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeScalarPtr NodeScalarSwitch::GetParameter() const
	{
		return m_pD->m_pParameter.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarSwitch::SetParameter( NodeScalarPtr pNode )
	{
		m_pD->m_pParameter = pNode;
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarSwitch::SetOptionCount( int t )
	{
		m_pD->m_options.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
    NodeScalarPtr NodeScalarSwitch::GetOption( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_options.Num() );
		return m_pD->m_options[t].get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarSwitch::SetOption( int t, NodeScalarPtr pNode )
	{
		check( t>=0 && t<(int)m_pD->m_options.Num() );
		m_pD->m_options[t] = pNode;
	}



}


