// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshConstantPrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshConstant::Private::s_type =
			NODE_TYPE( "MeshConstant", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshConstant, EType::Constant, Node, Node::EType::Mesh);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshConstant::GetInputCount() const
	{
		return m_pD->m_layouts.Num();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshConstant::GetInputNode( int i ) const
	{
		check( i>=0 && i<m_pD->m_layouts.Num() );
		return m_pD->m_layouts[i].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshConstant::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<m_pD->m_layouts.Num() );

		m_pD->m_layouts[i] = dynamic_cast<NodeLayout*>( pNode.get() );
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	MeshPtr NodeMeshConstant::GetValue() const
	{
		return m_pD->m_pValue;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshConstant::SetValue( MeshPtr pValue )
	{
		m_pD->m_pValue = pValue;

        if (m_pD->m_pValue)
        {
            // Ensure mesh is well formed
            m_pD->m_pValue->EnsureSurfaceData();
        }
    }


	//---------------------------------------------------------------------------------------------
	int NodeMeshConstant::GetLayoutCount() const
	{
		return m_pD->m_layouts.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshConstant::SetLayoutCount( int num )
	{
		check( num >=0 );
		m_pD->m_layouts.SetNum( num );
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshConstant::GetLayout( int index ) const
	{
		check( index >=0 && index < m_pD->m_layouts.Num() );

		return m_pD->GetLayout( index );
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshConstant::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if ( index >=0 && index < m_layouts.Num() )
		{
			pResult = m_layouts[ index ].get();
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshConstant::SetLayout( int index, NodeLayoutPtr pLayout )
	{
		check( index >=0 && index < m_pD->m_layouts.Num() );

		m_pD->m_layouts[ index ] = pLayout;
	}


}


