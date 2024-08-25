// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourSampleImage.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColourSampleImagePrivate.h"
#include "MuT/NodeImage.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeColourSampleImage::Private::s_type =
			FNodeType( "ColourSampleImage", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourSampleImage, EType::SampleImage, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourSampleImage::GetX() const
	{
		return m_pD->m_pX.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourSampleImage::SetX( NodeScalarPtr pNode )
	{
		m_pD->m_pX = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourSampleImage::GetY() const
	{
		return m_pD->m_pY.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourSampleImage::SetY( NodeScalarPtr pNode )
	{
		m_pD->m_pY = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeColourSampleImage::GetImage() const
	{
		return m_pD->m_pImage.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourSampleImage::SetImage( NodeImagePtr pNode )
	{
		m_pD->m_pImage = pNode;
	}



}
