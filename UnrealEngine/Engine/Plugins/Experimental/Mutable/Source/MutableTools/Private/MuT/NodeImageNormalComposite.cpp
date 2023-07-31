// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageNormalComposite.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageNormalCompositePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageNormalComposite::Private::s_type =
			NODE_TYPE( "ImageNormalComposite", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageNormalComposite, EType::NormalComposite, Node, Node::EType::Image )


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageNormalComposite::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageNormalComposite::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pBase.get(); break;
        case 1: pResult = m_pD->m_pNormal.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageNormalComposite::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pBase = dynamic_cast<NodeImage*>( pNode.get() ); break;
        case 1: m_pD->m_pNormal = dynamic_cast<NodeImage*>( pNode.get() ); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageNormalComposite::GetBase() const
	{
		return m_pD->m_pBase.get();
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageNormalComposite::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}

	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageNormalComposite::GetNormal() const
	{
		return m_pD->m_pNormal.get();
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageNormalComposite::SetNormal( NodeImagePtr pNode )
	{
		m_pD->m_pNormal = pNode;
	}

	//---------------------------------------------------------------------------------------------
	float NodeImageNormalComposite::GetPower() const
	{
		return m_pD->m_power;
	}

	
	//---------------------------------------------------------------------------------------------
	void NodeImageNormalComposite::SetPower( float power )
	{
		m_pD->m_power = power;
	}


	//---------------------------------------------------------------------------------------------
	ECompositeImageMode NodeImageNormalComposite::GetMode() const
	{
		return m_pD->m_mode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageNormalComposite::SetMode( ECompositeImageMode mode )
	{
		m_pD->m_mode = mode;
	}
}
