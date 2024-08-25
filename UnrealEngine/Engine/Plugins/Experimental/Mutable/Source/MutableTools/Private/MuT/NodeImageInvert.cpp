// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageInvert.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageInvertPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageInvert::Private::s_type =
		FNodeType("ImageInvert", NodeImage::GetStaticType());

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageInvert, EType::Invert, Node, Node::EType::Image)

	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageInvert::GetBase()const
	{
		return m_pD->m_pBase;
	}

	void NodeImageInvert::SetBase(NodeImagePtr pNode)
	{
		m_pD->m_pBase = pNode;
	}
	
}
