// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageTransform.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageTransformPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageTransform::Private::s_type =
			NODE_TYPE( "ImageTransform", NodeImage::GetStaticType() );

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageTransform, EType::Transform, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pBase = dynamic_cast<NodeImage*>(pNode.get()); break;
        case 1: m_pD->m_pOffsetX = dynamic_cast<NodeScalar*>(pNode.get()); break;
        case 2: m_pD->m_pOffsetY = dynamic_cast<NodeScalar*>(pNode.get()); break;
        case 3: m_pD->m_pScaleX = dynamic_cast<NodeScalar*>(pNode.get()); break;
        case 4: m_pD->m_pScaleY = dynamic_cast<NodeScalar*>(pNode.get()); break;
        case 5: m_pD->m_pRotation = dynamic_cast<NodeScalar*>(pNode.get()); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageTransform::GetInputCount() const
	{
		return 6;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageTransform::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pBase.get(); break;
		case 1: pResult = m_pD->m_pOffsetX.get(); break;
		case 2: pResult = m_pD->m_pOffsetY.get(); break;
		case 3: pResult = m_pD->m_pScaleX.get(); break;
		case 4: pResult = m_pD->m_pScaleY.get(); break;
		case 5: pResult = m_pD->m_pRotation.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageTransform::GetBase() const
	{
		return m_pD->m_pBase;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetOffsetX() const
	{
		return m_pD->m_pOffsetX;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetOffsetY() const
	{
		return m_pD->m_pOffsetY;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetScaleX() const
	{
		return m_pD->m_pScaleX;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetScaleY() const
	{
		return m_pD->m_pScaleY;
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageTransform::GetRotation() const
	{
		return m_pD->m_pRotation;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetOffsetX( NodeScalarPtr pNode )
	{
		m_pD->m_pOffsetX = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetOffsetY( NodeScalarPtr pNode )
	{
		m_pD->m_pOffsetY = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetScaleX( NodeScalarPtr pNode )
	{
		m_pD->m_pScaleX = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetScaleY( NodeScalarPtr pNode )
	{
		m_pD->m_pScaleY = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageTransform::SetRotation( NodeScalarPtr pNode )
	{
		m_pD->m_pRotation = pNode;
	}

}

