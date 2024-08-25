// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeSurfaceSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeSurfaceSwitchPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeSurfaceSwitch::Private::s_type =
			FNodeType( "SurfaceSwitch", NodeSurface::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeSurfaceSwitch, EType::Switch, Node, Node::EType::Surface)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeSurfaceSwitch::GetParameter() const
	{
		return m_pD->Parameter.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceSwitch::SetParameter( NodeScalarPtr pNode )
	{
		m_pD->Parameter = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceSwitch::SetOptionCount( int32 t )
	{
		m_pD->Options.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	NodeSurfacePtr NodeSurfaceSwitch::GetOption( int32 t ) const
	{
		check( t>=0 && t<m_pD->Options.Num() );
		return m_pD->Options[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceSwitch::SetOption( int32 t, NodeSurfacePtr pNode )
	{
		check( t>=0 && t<m_pD->Options.Num() );
		m_pD->Options[t] = pNode;
	}

}


