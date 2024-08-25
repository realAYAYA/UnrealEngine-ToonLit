// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourFromScalars.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColourFromScalarsPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeColourFromScalars::Private::s_type =
			FNodeType( "ColourFromScalars", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourFromScalars, EType::FromScalars, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourFromScalars::GetX() const
	{
		return m_pD->m_pX.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetX( NodeScalarPtr pNode )
	{
		m_pD->m_pX = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourFromScalars::GetY() const
	{
		return m_pD->m_pY.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetY( NodeScalarPtr pNode )
	{
		m_pD->m_pY = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourFromScalars::GetZ() const
	{
		return m_pD->m_pZ.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetZ( NodeScalarPtr pNode )
	{
		m_pD->m_pZ = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourFromScalars::GetW() const
	{
		return m_pD->m_pW.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetW( NodeScalarPtr pNode )
	{
		m_pD->m_pW = pNode;
	}

}

