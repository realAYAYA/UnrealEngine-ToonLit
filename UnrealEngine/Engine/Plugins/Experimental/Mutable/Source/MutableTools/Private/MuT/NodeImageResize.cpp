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
	NODE_TYPE NodeImageResize::Private::s_type =
			NODE_TYPE( "ImageResize", NodeImage::GetStaticType() );

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageResize, EType::Resize, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	void NodeImageResize::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pBase = dynamic_cast<NodeImage*>(pNode.get()); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageResize::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageResize::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pBase.get(); break;
		}

		return pResult;
	}


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
