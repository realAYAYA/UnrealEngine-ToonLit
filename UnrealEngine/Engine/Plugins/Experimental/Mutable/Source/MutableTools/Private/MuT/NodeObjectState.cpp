// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeObjectState.h"

#include "HAL/PlatformString.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeObjectPrivate.h"
#include "MuT/NodeObjectStatePrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeObjectState::Private::s_type =
			NODE_TYPE( "ObjectState", NodeObjectState::GetStaticType() );


	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeObjectState::Private::GetLayout(int lod, int component, int surface, int texture ) const
	{
		NodeLayoutPtr pResult;

		if ( m_pSource )
		{
			const NodeObject::Private* pPrivate = dynamic_cast<const NodeObject::Private*>
					( m_pSource->GetBasePrivate() );
            pResult = pPrivate->GetLayout( lod, component, surface, texture );
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeObjectState, NodeObject::EType::State, Node, Node::EType::Object)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeObjectState::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeObjectState::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		if (i==0)
		{
			pResult = m_pD->m_pSource.get();
		}
		else
		{
			pResult = m_pD->m_pRoot.get();
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectState::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		if (i==0)
		{
			m_pD->m_pSource = dynamic_cast<NodeObject*>( pNode.get() );
		}
		else
		{
			m_pD->m_pRoot = dynamic_cast<NodeObject*>( pNode.get() );
		}
	}


	//---------------------------------------------------------------------------------------------
	// NodeObject Interface
	//---------------------------------------------------------------------------------------------
	const char* NodeObjectState::GetName() const
	{
		const char* strRes = "";
		return strRes;
	}


	//---------------------------------------------------------------------------------------------
    void NodeObjectState::SetName( const char* )
	{
	}


	const char* NodeObjectState::GetUid() const
	{
		const char* strRes = "";
		return strRes;
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectState::SetUid( const char* )
	{
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeObjectPtr NodeObjectState::GetSource() const
	{
		return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectState::SetSource( NodeObjectPtr pSource )
	{
		m_pD->m_pSource = pSource;
	}


	//---------------------------------------------------------------------------------------------
	NodeObjectPtr NodeObjectState::GetStateRoot() const
	{
		return m_pD->m_pRoot.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectState::SetStateRoot( NodeObjectPtr pState )
	{
		m_pD->m_pRoot = pState;
	}


	//---------------------------------------------------------------------------------------------
	const char* NodeObjectState::GetStateName() const
	{
		return m_pD->m_state.m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectState::SetStateName( const char* n )
	{
		m_pD->m_state.m_name = n;
	}


	//---------------------------------------------------------------------------------------------
	bool NodeObjectState::HasStateParam( const char* param ) const
	{
		return m_pD->m_state.m_runtimeParams.Contains( param );
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectState::AddStateParam( const char* param )
	{
		if (!HasStateParam(param))
		{
			m_pD->m_state.m_runtimeParams.Add( param );
		}
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectState::RemoveStateParam( const char* param )
	{
		int32 it = m_pD->m_state.m_runtimeParams.Find( param );
		if ( it != INDEX_NONE )
		{
			m_pD->m_state.m_runtimeParams.RemoveAt( it );
		}
	}

}


