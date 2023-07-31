// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeColourSwitchPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"

#include <memory>
#include <utility>

namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeColourSwitch::Private::s_type =
            NODE_TYPE( "ColourSwitch", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeColourSwitch, EType::Switch, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeColourSwitch::GetInputCount() const
	{
		return 1 + m_pD->m_options.Num();
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeColourSwitch::GetInputNode( int i ) const
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
    void NodeColourSwitch::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pParameter = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		default:
            m_pD->m_options[i-1] = dynamic_cast<NodeColour*>(pNode.get());
			break;
		}
	}


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


