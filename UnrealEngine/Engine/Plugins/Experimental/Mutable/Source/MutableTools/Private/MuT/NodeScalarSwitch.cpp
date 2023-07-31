// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarSwitchPrivate.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeScalarSwitch::Private::s_type =
            NODE_TYPE( "ScalarSwitch", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeScalarSwitch, EType::Switch, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeScalarSwitch::GetInputCount() const
	{
		return 1 + (int)m_pD->m_options.Num();
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeScalarSwitch::GetInputNode( int i ) const
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
    void NodeScalarSwitch::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pParameter = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		default:
            m_pD->m_options[i-1] = dynamic_cast<NodeScalar*>(pNode.get());
			break;
		}
	}


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


