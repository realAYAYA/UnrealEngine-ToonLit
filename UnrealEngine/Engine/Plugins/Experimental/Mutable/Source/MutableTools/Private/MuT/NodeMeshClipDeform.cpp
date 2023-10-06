// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeMeshClipDeform.h"

#include "Misc/AssertionMacros.h"
#include "MuR/RefCounted.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshClipDeformPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshClipDeform::Private::s_type =
			NODE_TYPE( "MeshClipDeform", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshClipDeform, EType::ClipDeform, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshClipDeform::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshClipDeform::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = nullptr;

		switch (i)
		{
		case 0: pResult = m_pD->m_pBaseMesh.get(); break;
		case 1: pResult = m_pD->m_pClipShape.get(); break;
		default:
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshClipDeform::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0: m_pD->m_pBaseMesh = dynamic_cast<NodeMesh*>(pNode.get()); break;
		case 1: m_pD->m_pClipShape = dynamic_cast<NodeMesh*>(pNode.get()); break;
		default:
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const Ptr<NodeMesh>& NodeMeshClipDeform::GetBaseMesh() const
	{
		return m_pD->m_pBaseMesh;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshClipDeform::SetBaseMesh(const Ptr<NodeMesh>& pNode)
	{
		m_pD->m_pBaseMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
	const Ptr<NodeMesh>& NodeMeshClipDeform::GetClipShape() const
	{
		return m_pD->m_pClipShape;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshClipDeform::SetClipShape(const Ptr<NodeMesh>& pNode)
	{
		m_pD->m_pClipShape = pNode;
	}


	//---------------------------------------------------------------------------------------------
	Ptr<NodeLayout> NodeMeshClipDeform::Private::GetLayout( int index ) const
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


