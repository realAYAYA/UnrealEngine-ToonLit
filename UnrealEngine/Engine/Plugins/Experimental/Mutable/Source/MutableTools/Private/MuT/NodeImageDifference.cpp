// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageDifference.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageDifferencePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageDifference::Private::s_type =
			NODE_TYPE( "NodeImageDifference", NodeImage::GetStaticType() );

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageDifference, EType::Difference, Node, Node::EType::Image);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	void NodeImageDifference::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pA = dynamic_cast<NodeImage*>(pNode.get()); break;
        case 1: m_pD->m_pB = dynamic_cast<NodeImage*>(pNode.get()); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageDifference::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageDifference::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pA.get(); break;
        case 1: pResult = m_pD->m_pB.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageDifference::GetA() const
	{
		return m_pD->m_pA;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageDifference::SetA( NodeImagePtr pNode )
	{
		m_pD->m_pA = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageDifference::GetB() const
	{
		return m_pD->m_pB;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageDifference::SetB( NodeImagePtr pNode )
	{
		m_pD->m_pB = pNode;
	}

}


