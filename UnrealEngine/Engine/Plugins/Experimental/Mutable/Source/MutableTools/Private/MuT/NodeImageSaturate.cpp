// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageSaturate.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageSaturatePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageSaturate::Private::s_type =
			FNodeType( "ImageSaturate", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageSaturate, EType::Saturate, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageSaturate::GetFactor() const
	{
		return m_pD->m_pFactor.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSaturate::SetFactor( NodeScalarPtr pNode )
	{
		m_pD->m_pFactor = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageSaturate::GetSource() const
	{
		return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSaturate::SetSource( NodeImagePtr pNode )
	{
		m_pD->m_pSource = pNode;
	}



}

