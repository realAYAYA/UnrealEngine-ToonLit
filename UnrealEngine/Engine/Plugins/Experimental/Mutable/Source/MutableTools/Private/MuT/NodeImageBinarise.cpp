// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageBinarise.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageBinarisePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageBinarise::Private::s_type =
			NODE_TYPE( "ImageMultiply", NodeImage::GetStaticType() );

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageBinarise, EType::Binarise, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	void NodeImageBinarise::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pBase = dynamic_cast<NodeImage*>(pNode.get()); break;
        case 1: m_pD->m_pThreshold = dynamic_cast<NodeScalar*>(pNode.get()); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageBinarise::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageBinarise::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pBase.get(); break;
        case 1: pResult = m_pD->m_pThreshold.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageBinarise::GetBase() const
	{
		return m_pD->m_pBase;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageBinarise::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageBinarise::GetThreshold() const
	{
		return m_pD->m_pThreshold;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageBinarise::SetThreshold( NodeScalarPtr pNode )
	{
		m_pD->m_pThreshold = pNode;
	}

}

