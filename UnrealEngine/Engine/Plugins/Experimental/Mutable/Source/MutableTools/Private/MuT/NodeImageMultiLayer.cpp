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
    FNodeType NodeImageMultiLayer::Private::s_type =
            FNodeType( "ImageMultiLayer", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageMultiLayer, EType::MultiLayer, Node, Node::EType::Image)


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
