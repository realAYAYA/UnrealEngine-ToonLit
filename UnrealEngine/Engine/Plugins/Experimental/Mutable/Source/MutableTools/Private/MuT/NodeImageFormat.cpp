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
	NODE_TYPE NodeImageFormat::Private::s_type =
			NODE_TYPE( "ImageFormat", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageFormat, EType::Format, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageFormat::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageFormat::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );
        (void)i;

		return m_pD->m_source.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageFormat::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );
        (void)i;

		m_pD->m_source = dynamic_cast<NodeImage*>(pNode.get());
	}


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
