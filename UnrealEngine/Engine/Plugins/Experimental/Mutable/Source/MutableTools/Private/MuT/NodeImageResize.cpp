// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageResize.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageResizePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageResize::Private::s_type =
			FNodeType( "ImageResize", NodeImage::GetStaticType() );

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageResize, EType::Resize, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageResize::GetBase() const
	{
		return m_pD->m_pBase;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageResize::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageResize::SetRelative( bool rel )
	{
		m_pD->m_relative = rel;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageResize::SetSize( float x, float y )
	{
		m_pD->m_sizeX = x;
		m_pD->m_sizeY = y;
	}

}
