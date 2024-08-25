// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageFormat.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageFormatPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageFormat::Private::s_type =
			FNodeType( "ImageFormat", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageFormat, EType::Format, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	EImageFormat NodeImageFormat::GetFormat() const
	{
		return m_pD->m_format;
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageFormat::SetFormat(EImageFormat format, EImageFormat formatIfAlpha )
	{
		m_pD->m_format = format;
        m_pD->m_formatIfAlpha = formatIfAlpha;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageFormat::GetSource() const
	{
		return m_pD->m_source.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageFormat::SetSource(NodeImagePtr pNode )
	{
		m_pD->m_source = pNode;
	}

}
