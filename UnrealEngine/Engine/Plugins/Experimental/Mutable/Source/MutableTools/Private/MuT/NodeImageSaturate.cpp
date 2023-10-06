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
	NODE_TYPE NodeImageSaturate::Private::s_type =
			NODE_TYPE( "ImageSaturate", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageSaturate, EType::Saturate, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageSaturate::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageSaturate::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
		case 0: pResult = m_pD->m_pFactor.get();
		case 1: pResult = m_pD->m_pSource.get();
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSaturate::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
		case 0: m_pD->m_pFactor = dynamic_cast<NodeScalar*>(pNode.get());
		case 1: m_pD->m_pSource = dynamic_cast<NodeImage*>(pNode.get());
		}
	}


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

