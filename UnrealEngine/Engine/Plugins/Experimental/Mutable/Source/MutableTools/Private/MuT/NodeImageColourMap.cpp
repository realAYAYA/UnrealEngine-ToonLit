// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageColourMap.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageColourMapPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageColourMap::Private::s_type =
			NODE_TYPE( "ImageColourMap", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageColourMap, EType::ColourMap, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageColourMap::GetInputCount() const
	{
		return 3;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageColourMap::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pBase.get(); break;
        case 1: pResult = m_pD->m_pMask.get(); break;
        case 2: pResult = m_pD->m_pMap.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageColourMap::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
        case 0: m_pD->m_pBase = dynamic_cast<NodeImage*>( pNode.get() ); break;
        case 1: m_pD->m_pMask = dynamic_cast<NodeImage*>( pNode.get() ); break;
        case 2: m_pD->m_pMap = dynamic_cast<NodeImage*>( pNode.get() ); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageColourMap::GetBase() const
	{
		return m_pD->m_pBase.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageColourMap::SetBase( NodeImagePtr pNode )
	{
		m_pD->m_pBase = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageColourMap::GetMap() const
	{
		return m_pD->m_pMap.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageColourMap::SetMap( NodeImagePtr pNode )
	{
		m_pD->m_pMap = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageColourMap::GetMask() const
	{
		return m_pD->m_pMask.get();
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageColourMap::SetMask( NodeImagePtr pNode )
	{
		m_pD->m_pMask = pNode;
	}

}
