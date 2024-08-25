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
	FNodeType NodeMeshClipDeform::Private::s_type =
			FNodeType( "MeshClipDeform", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshClipDeform, EType::ClipDeform, Node, Node::EType::Mesh)


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
			NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>(m_pBaseMesh->GetBasePrivate() );
			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}


