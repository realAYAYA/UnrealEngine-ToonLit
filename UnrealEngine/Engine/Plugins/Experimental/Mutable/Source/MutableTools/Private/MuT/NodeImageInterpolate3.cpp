// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageInterpolate3.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageInterpolate3Private.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageInterpolate3::Private::s_type =
			NODE_TYPE( "ImageInterpolate3", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageInterpolate3, EType::Interpolate3, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageInterpolate3::GetInputCount() const
	{
		return 5;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageInterpolate3::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		switch (i)
		{
		case 0: pResult = m_pD->m_pFactor1.get(); break;
		case 1: pResult = m_pD->m_pFactor2.get(); break;
		case 2: pResult = m_pD->m_pTarget0.get(); break;
		case 3: pResult = m_pD->m_pTarget1.get(); break;
		case 4: pResult = m_pD->m_pTarget2.get(); break;
		default:
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate3::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0: m_pD->m_pFactor1 = dynamic_cast<NodeScalar*>(pNode.get()); break;
		case 1: m_pD->m_pFactor2 = dynamic_cast<NodeScalar*>(pNode.get()); break;
		case 2: m_pD->m_pTarget0 = dynamic_cast<NodeImage*>(pNode.get()); break;
		case 3: m_pD->m_pTarget1 = dynamic_cast<NodeImage*>(pNode.get()); break;
		case 4: m_pD->m_pTarget2 = dynamic_cast<NodeImage*>(pNode.get()); break;
		default:
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageInterpolate3::GetFactor1() const
	{
		return m_pD->m_pFactor1.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate3::SetFactor1( NodeScalarPtr pNode )
	{
		m_pD->m_pFactor1 = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageInterpolate3::GetFactor2() const
	{
		return m_pD->m_pFactor2.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate3::SetFactor2( NodeScalarPtr pNode )
	{
		m_pD->m_pFactor2 = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageInterpolate3::GetTarget0() const
	{
		return m_pD->m_pTarget0.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate3::SetTarget0( NodeImagePtr pNode )
	{
		m_pD->m_pTarget0 = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageInterpolate3::GetTarget1() const
	{
		return m_pD->m_pTarget1.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate3::SetTarget1( NodeImagePtr pNode )
	{
		m_pD->m_pTarget1 = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageInterpolate3::GetTarget2() const
	{
		return m_pD->m_pTarget2.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate3::SetTarget2( NodeImagePtr pNode )
	{
		m_pD->m_pTarget2 = pNode;
	}



}


