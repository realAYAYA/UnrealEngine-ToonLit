// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageInterpolate.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageInterpolatePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageInterpolate::Private::s_type =
			FNodeType( "ImageInterpolate", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageInterpolate, EType::Interpolate, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageInterpolate::GetFactor() const
	{
		return m_pD->m_pFactor.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate::SetFactor( NodeScalarPtr pNode )
	{
		m_pD->m_pFactor = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageInterpolate::GetTarget( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_targets.Num() );
		return m_pD->m_targets[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate::SetTarget( int t, NodeImagePtr pNode )
	{
		check( t>=0 && t<(int)m_pD->m_targets.Num() );
		m_pD->m_targets[t] = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate::SetTargetCount( int t )
	{
		m_pD->m_targets.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageInterpolate::GetTargetCount() const
	{
		return int(m_pD->m_targets.Num());
	}



}


