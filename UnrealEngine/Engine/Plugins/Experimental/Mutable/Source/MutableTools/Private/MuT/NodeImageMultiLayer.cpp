// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageMultiLayer.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageMultiLayerPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeImageMultiLayer::Private::s_type =
            NODE_TYPE( "ImageMultiLayer", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageMultiLayer, EType::MultiLayer, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeImageMultiLayer::GetInputCount() const
	{
		return 4;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeImageMultiLayer::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pBase.get(); break;
        case 1: pResult = m_pD->m_pMask.get(); break;
        case 2: pResult = m_pD->m_pBlended.get(); break;
        case 3: pResult = m_pD->m_pRange.get(); break;
        }

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageMultiLayer::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pBase = dynamic_cast<NodeImage*>( pNode.get() ); break;
        case 1: m_pD->m_pMask = dynamic_cast<NodeImage*>( pNode.get() ); break;
        case 2: m_pD->m_pBlended = dynamic_cast<NodeImage*>( pNode.get() ); break;
        case 3: m_pD->m_pRange = dynamic_cast<NodeRange*>( pNode.get() ); break;
        }
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeImagePtr NodeImageMultiLayer::GetBase() const
	{
		return m_pD->m_pBase.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageMultiLayer::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}


	//---------------------------------------------------------------------------------------------
    NodeImagePtr NodeImageMultiLayer::GetMask() const
	{
		return m_pD->m_pMask.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageMultiLayer::SetMask( NodeImagePtr pNode )
	{
		m_pD->m_pMask = pNode;
	}


    //---------------------------------------------------------------------------------------------
    NodeImagePtr NodeImageMultiLayer::GetBlended() const
    {
        return m_pD->m_pBlended.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeImageMultiLayer::SetBlended( NodeImagePtr pNode )
    {
        m_pD->m_pBlended = pNode;
    }


    //---------------------------------------------------------------------------------------------
    NodeRangePtr NodeImageMultiLayer::GetRange() const
    {
        return m_pD->m_pRange.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeImageMultiLayer::SetRange( NodeRangePtr pNode )
    {
        m_pD->m_pRange = pNode;
    }


	//---------------------------------------------------------------------------------------------
	EBlendType NodeImageMultiLayer::GetBlendType() const
	{
		return m_pD->m_type;
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageMultiLayer::SetBlendType(EBlendType t)
	{
		m_pD->m_type = t;
	}


}
