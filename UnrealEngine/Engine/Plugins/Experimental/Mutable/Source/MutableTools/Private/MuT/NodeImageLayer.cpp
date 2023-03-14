// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageLayer.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageLayerPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageLayer::Private::s_type =
			NODE_TYPE( "ImageLayer", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageLayer, EType::Layer, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	const char* NodeImageLayer::s_blendTypeName[] =
	{
		"SoftLight",
		"HardLight",
		"Burn",
		"Dodge",
		"Screen",
		"Overlay",
		"Blend",
        "Multiply",
        "AlphaOverlay"
	};


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageLayer::GetInputCount() const
	{
		return 3;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageLayer::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pBase.get(); break;
        case 1: pResult = m_pD->m_pMask.get(); break;
        case 2: pResult = m_pD->m_pBlended.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLayer::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pBase = dynamic_cast<NodeImage*>( pNode.get() ); break;
        case 1: m_pD->m_pMask = dynamic_cast<NodeImage*>( pNode.get() ); break;
        case 2: m_pD->m_pBlended = dynamic_cast<NodeImage*>( pNode.get() ); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageLayer::GetBase() const
	{
		return m_pD->m_pBase.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLayer::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageLayer::GetMask() const
	{
		return m_pD->m_pMask.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLayer::SetMask( NodeImagePtr pNode )
	{
		m_pD->m_pMask = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageLayer::GetBlended() const
	{
		return m_pD->m_pBlended.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLayer::SetBlended( NodeImagePtr pNode )
	{
		m_pD->m_pBlended = pNode;
	}


	//---------------------------------------------------------------------------------------------
	EBlendType NodeImageLayer::GetBlendType() const
	{
		return m_pD->m_type;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLayer::SetBlendType(EBlendType t)
	{
		m_pD->m_type = t;
	}


}
