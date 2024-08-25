// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshMakeMorph.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshMakeMorphPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeMeshMakeMorph::Private::s_type =
			FNodeType( "MeshMakeMorph", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshMakeMorph, EType::MakeMorph, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshMakeMorph::GetBase() const
	{
		return m_pD->m_pBase;
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshMakeMorph::SetBase( NodeMesh* p )
	{
		m_pD->m_pBase = p;
	}


	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshMakeMorph::GetTarget() const
	{
		return m_pD->m_pTarget;
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshMakeMorph::SetTarget( NodeMesh* p )
	{
		m_pD->m_pTarget = p;
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshMakeMorph::SetOnlyPositionAndNormal(bool bInOnlyPositionAndNormals)
	{
		m_pD->bOnlyPositionAndNormal = bInOnlyPositionAndNormals;
	}


	//---------------------------------------------------------------------------------------------
	bool NodeMeshMakeMorph::GetOnlyPositionAndNormal() const
	{
		return m_pD->bOnlyPositionAndNormal;
	}

	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeMeshMakeMorph::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		// TODO: Substract layouts too? Usually they are ignored.
		if ( m_pBase )
		{
			NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>( m_pBase->GetBasePrivate() );
			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}
