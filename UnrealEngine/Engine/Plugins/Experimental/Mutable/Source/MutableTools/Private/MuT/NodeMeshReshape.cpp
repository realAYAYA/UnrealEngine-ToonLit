// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeMeshReshape.h"

#include "Containers/Array.h"
#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshReshapePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshReshape::Private::s_type =
			NODE_TYPE( "MeshReshape", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshReshape, EType::Reshape, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshReshape::GetInputCount() const
	{
		return 3;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshReshape::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = nullptr;

		switch (i)
		{
		case 0: pResult = m_pD->m_pBaseMesh.get(); break;
		case 1: pResult = m_pD->m_pBaseShape.get(); break;
		case 2: pResult = m_pD->m_pTargetShape.get(); break;
		default:
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0: m_pD->m_pBaseMesh = dynamic_cast<NodeMesh*>(pNode.get()); break;
		case 1: m_pD->m_pBaseShape = dynamic_cast<NodeMesh*>(pNode.get()); break;
		case 2: m_pD->m_pTargetShape = dynamic_cast<NodeMesh*>(pNode.get()); break;
		default:
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const Ptr<NodeMesh>& NodeMeshReshape::GetBaseMesh() const
	{
		return m_pD->m_pBaseMesh;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::SetBaseMesh(const Ptr<NodeMesh>& pNode)
	{
		m_pD->m_pBaseMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
	const Ptr<NodeMesh>& NodeMeshReshape::GetBaseShape() const
	{
		return m_pD->m_pBaseShape;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::SetBaseShape(const Ptr<NodeMesh>& pNode)
	{
		m_pD->m_pBaseShape = pNode;
	}


	//---------------------------------------------------------------------------------------------
	const Ptr<NodeMesh>& NodeMeshReshape::GetTargetShape() const
	{
		return m_pD->m_pTargetShape;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::SetTargetShape(const Ptr<NodeMesh>& pNode)
	{
		m_pD->m_pTargetShape = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::SetReshapeVertices(bool bEnable)
	{
		m_pD->m_reshapeVertices = bEnable;
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::SetReshapeSkeleton(bool bEnable)
	{
		m_pD->m_reshapeSkeleton = bEnable;
	}
	

	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::SetColorUsages(EVertexColorUsage R, EVertexColorUsage G, EVertexColorUsage B, EVertexColorUsage A)
	{	
		m_pD->ColorRChannelUsage = R;
		m_pD->ColorGChannelUsage = G;
		m_pD->ColorBChannelUsage = B;
		m_pD->ColorAChannelUsage = A;
	}	

	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::AddBoneToDeform(const uint16 BoneId)
	{
		m_pD->BonesToDeform.Emplace(BoneId);
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshReshape::AddPhysicsBodyToDeform(const uint16 BoneId)
	{
		m_pD->PhysicsToDeform.Emplace(BoneId);
	}

	void NodeMeshReshape::SetReshapePhysicsVolumes(bool bEnable)
	{
		m_pD->m_reshapePhysicsVolumes = bEnable;
	}
	
	//---------------------------------------------------------------------------------------------
	Ptr<NodeLayout> NodeMeshReshape::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if (m_pBaseMesh)
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>(m_pBaseMesh->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}


