// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeImageSwitchPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageSwitch::Private::s_type =
			NODE_TYPE( "ImageSwitch", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageSwitch, EType::Switch, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageSwitch::GetInputCount() const
	{
		return 1 + (int)m_pD->m_options.Num();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageSwitch::GetInputNode( int i ) const
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
	void NodeImageSwitch::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pParameter = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		default:
			m_pD->m_options[i-1] = dynamic_cast<NodeImage*>(pNode.get());
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageSwitch::GetParameter() const
	{
		return m_pD->m_pParameter.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwitch::SetParameter( NodeScalarPtr pNode )
	{
		m_pD->m_pParameter = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwitch::SetOptionCount( int t )
	{
		m_pD->m_options.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageSwitch::GetOptionCount() const 
	{
		return int(m_pD->m_options.Num());
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageSwitch::GetOption( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_options.Num() );
		return m_pD->m_options[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwitch::SetOption( int t, NodeImagePtr pNode )
	{
		check( t>=0 && t<(int)m_pD->m_options.Num() );
		m_pD->m_options[t] = pNode;
	}



}


