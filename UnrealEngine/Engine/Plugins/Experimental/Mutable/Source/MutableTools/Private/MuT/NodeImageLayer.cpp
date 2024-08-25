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
	FNodeType NodeImageLayer::Private::s_type =
			FNodeType( "ImageLayer", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageLayer, EType::Layer, Node, Node::EType::Image)


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
