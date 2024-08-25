// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageNormalComposite.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageNormalCompositePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuR/Image.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageNormalComposite::Private::s_type =
			FNodeType( "ImageNormalComposite", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageNormalComposite, EType::NormalComposite, Node, Node::EType::Image )


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
