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
	FNodeType NodeImageColourMap::Private::s_type =
			FNodeType( "ImageColourMap", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageColourMap, EType::ColourMap, Node, Node::EType::Image)


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
