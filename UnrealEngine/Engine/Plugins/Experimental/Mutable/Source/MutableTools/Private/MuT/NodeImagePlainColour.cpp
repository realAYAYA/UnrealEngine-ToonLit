// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImagePlainColour.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeImagePlainColourPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImagePlainColour::Private::s_type =
			FNodeType( "ImagePlainColour", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImagePlainColour, EType::PlainColour, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeImagePlainColour::GetColour() const
	{
		return m_pD->m_pColour;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImagePlainColour::SetColour( NodeColourPtr pNode )
	{
		m_pD->m_pColour = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImagePlainColour::SetSize( int x, int y )
	{
		m_pD->m_sizeX = x;
		m_pD->m_sizeY = y;
	}

}

