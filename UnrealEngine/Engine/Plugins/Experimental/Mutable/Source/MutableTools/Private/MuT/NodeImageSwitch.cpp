// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageSwitchPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageSwitch::Private::s_type =
			FNodeType( "ImageSwitch", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageSwitch, EType::Switch, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageSwitch::GetParameter() const
	{
		return m_pD->m_pParameter.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwitch::SetParameter( NodeScalarPtr pNode )
	{
		m_pD->m_pParameter = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwitch::SetOptionCount( int t )
	{
		m_pD->m_options.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageSwitch::GetOptionCount() const 
	{
		return int(m_pD->m_options.Num());
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageSwitch::GetOption( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_options.Num() );
		return m_pD->m_options[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwitch::SetOption( int t, NodeImagePtr pNode )
	{
		check( t>=0 && t<(int)m_pD->m_options.Num() );
		m_pD->m_options[t] = pNode;
	}



}


