// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageLayerColour.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeImageLayerColourPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageLayerColour::Private::s_type =
			FNodeType( "ImageLayerColour", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageLayerColour, EType::LayerColour, Node, Node::EType::Image);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageLayerColour::GetBase() const
	{
		return m_pD->m_pBase.get();
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageLayerColour::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}

	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageLayerColour::GetMask() const
	{
		return m_pD->m_pMask.get();
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageLayerColour::SetMask( NodeImagePtr pNode )
	{
		m_pD->m_pMask = pNode;
	}

	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeImageLayerColour::GetColour() const
	{
		return m_pD->m_pColour.get();
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageLayerColour::SetColour( NodeColourPtr pNode )
	{
		m_pD->m_pColour = pNode;
	}


	//---------------------------------------------------------------------------------------------
	EBlendType NodeImageLayerColour::GetBlendType() const
	{
		return m_pD->m_type;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLayerColour::SetBlendType(EBlendType t)
	{
		m_pD->m_type = t;
	}

}

