// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageInterpolate.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeImageInterpolatePrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageInterpolate::Private::s_type =
			NODE_TYPE( "ImageInterpolate", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageInterpolate, EType::Interpolate, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageInterpolate::GetInputCount() const
	{
		return 1 + (int)m_pD->m_targets.Num();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageInterpolate::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		switch (i)
		{
		case 0:
			pResult = m_pD->m_pFactor.get();
			break;

		default:
			pResult = m_pD->m_targets[i-1].get();
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageInterpolate::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pFactor = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		default:
			m_pD->m_targets[i-1] = dynamic_cast<NodeImage*>(pNode.get());
			break;
		}
	}


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


