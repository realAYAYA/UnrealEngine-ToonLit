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
    NODE_TYPE NodeMeshApplyPose::Private::s_type =
            NODE_TYPE( "MeshApplyPose", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshApplyPose, EType::ApplyPose, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeMeshApplyPose::GetInputCount() const
	{
        return 2;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeMeshApplyPose::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		switch (i)
		{
		case 0:
            pResult = m_pD->m_pBase.get();
            break;
        case 1:
            pResult = m_pD->m_pPose.get();
			break;
        default:
            check(false);
            break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshApplyPose::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
            m_pD->m_pBase = dynamic_cast<NodeMesh*>(pNode.get());
            break;

		case 1:
            m_pD->m_pPose = dynamic_cast<NodeMesh*>(pNode.get());
			break;

        default:
            check(false);
            break;
        }
	}


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
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_pBase->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}


