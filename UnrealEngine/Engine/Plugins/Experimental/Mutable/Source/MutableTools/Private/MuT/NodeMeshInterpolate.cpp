// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshInterpolate.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshInterpolatePrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeMeshInterpolate::Private::s_type =
			FNodeType( "MeshInterpolate", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshInterpolate, EType::Interpolate, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeMeshInterpolate::GetFactor() const
	{
		return m_pD->m_pFactor.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetFactor( NodeScalarPtr pNode )
	{
		m_pD->m_pFactor = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMeshInterpolate::GetTarget( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_targets.Num() );

		return m_pD->m_targets[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetTarget( int t, NodeMeshPtr pNode )
	{
		check( t>=0 && t<(int)m_pD->m_targets.Num() );
		m_pD->m_targets[t] = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetTargetCount( int t )
	{
		m_pD->m_targets.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	int NodeMeshInterpolate::GetTargetCount() const
	{
		return m_pD->m_targets.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetChannelCount( int t )
	{
		m_pD->m_channels.SetNum(t);
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetChannel( int i, MESH_BUFFER_SEMANTIC semantic, int semanticIndex )
	{
		check( i>=0 && i<m_pD->m_channels.Num() );

		if ( i>=0 && i<m_pD->m_channels.Num() )
		{
			m_pD->m_channels[i].semantic = semantic;
			m_pD->m_channels[i].semanticIndex = semanticIndex;
		}
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshInterpolate::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		// TODO: Substract layouts too? Usually they are ignored.
		if ( m_targets.Num()>0 && m_targets[0] )
		{
			NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>( m_targets[0]->GetBasePrivate() );
			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}
