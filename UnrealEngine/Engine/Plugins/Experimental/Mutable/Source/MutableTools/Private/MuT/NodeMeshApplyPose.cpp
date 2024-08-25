// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeMeshApplyPose.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshApplyPosePrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeMeshApplyPose::Private::s_type =
            FNodeType( "MeshApplyPose", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshApplyPose, EType::ApplyPose, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshApplyPose::GetBase() const
    {
        return m_pD->m_pBase.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshApplyPose::SetBase( NodeMeshPtr pNode )
    {
        m_pD->m_pBase = pNode;
    }


    //---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshApplyPose::GetPose() const
    {
        return m_pD->m_pPose.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshApplyPose::SetPose( NodeMeshPtr pNode )
    {
        m_pD->m_pPose = pNode;
    }


	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeMeshApplyPose::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if ( m_pBase )
		{
			NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>( m_pBase->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}


