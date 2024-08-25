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
	FNodeType NodeImageBinarise::Private::s_type =
			FNodeType( "ImageMultiply", NodeImage::GetStaticType() );

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageBinarise, EType::Binarise, Node, Node::EType::Image)


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

