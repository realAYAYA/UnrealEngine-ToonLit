// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColourSwitchPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeColourSwitch::Private::s_type =
            FNodeType( "ColourSwitch", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeColourSwitch, EType::Switch, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeScalarPtr NodeColourSwitch::GetParameter() const
	{
		return m_pD->m_pParameter.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeColourSwitch::SetParameter( NodeScalarPtr pNode )
	{
		m_pD->m_pParameter = pNode;
	}


	//---------------------------------------------------------------------------------------------
    void NodeColourSwitch::SetOptionCount( int t )
	{
		m_pD->m_options.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
    NodeColourPtr NodeColourSwitch::GetOption( int t ) const
	{
		check( t>=0 && t<m_pD->m_options.Num() );
		return m_pD->m_options[t].get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeColourSwitch::SetOption( int t, NodeColourPtr pNode )
	{
		check( t>=0 && t<m_pD->m_options.Num() );
		m_pD->m_options[t] = pNode;
	}



}


