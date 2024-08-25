// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageGradient.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeImageGradientPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageGradient::Private::s_type =
			FNodeType( "ImageGradient", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageGradient, EType::Gradient, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeImageGradient::GetColour0() const
	{
		return m_pD->m_pColour0.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageGradient::SetColour0( NodeColourPtr pNode )
	{
		m_pD->m_pColour0 = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeImageGradient::GetColour1() const
	{
		return m_pD->m_pColour1.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageGradient::SetColour1( NodeColourPtr pNode )
	{
		m_pD->m_pColour1 = pNode;
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageGradient::GetSizeX() const
	{
		return m_pD->m_size[0];
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageGradient::GetSizeY() const
	{
		return m_pD->m_size[1];
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageGradient::SetSize( int x, int y )
	{
		m_pD->m_size[0] = x;
		m_pD->m_size[1] = y;
	}

}

