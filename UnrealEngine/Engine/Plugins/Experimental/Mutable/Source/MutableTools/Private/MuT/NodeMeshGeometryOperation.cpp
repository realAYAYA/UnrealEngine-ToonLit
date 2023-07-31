// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshGeometryOperation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshGeometryOperationPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"



namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshGeometryOperation::Private::s_type =
			NODE_TYPE( "MeshGeometryOperation", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshGeometryOperation, EType::GeometryOperation, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshGeometryOperation::GetInputCount() const
	{
		return 4;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshGeometryOperation::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = nullptr;

		switch (i)
		{
		case 0: pResult = m_pD->m_pMeshA.get(); break;
		case 1: pResult = m_pD->m_pMeshB.get(); break;
		case 2: pResult = m_pD->m_pScalarA.get(); break;
		case 3: pResult = m_pD->m_pScalarB.get(); break;
		default:
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshGeometryOperation::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0: m_pD->m_pMeshA = dynamic_cast<NodeMesh*>(pNode.get()); break;
		case 1: m_pD->m_pMeshB = dynamic_cast<NodeMesh*>(pNode.get()); break;
		case 2: m_pD->m_pScalarA = dynamic_cast<NodeScalar*>(pNode.get()); break;
		case 3: m_pD->m_pScalarB = dynamic_cast<NodeScalar*>(pNode.get()); break;
		default:
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const NodeScalarPtr& NodeMeshGeometryOperation::GetScalarA() const
	{
		return m_pD->m_pScalarA;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshGeometryOperation::SetScalarA(const NodeScalarPtr& pNode)
	{
		m_pD->m_pScalarA = pNode;
	}


	//---------------------------------------------------------------------------------------------
	const NodeScalarPtr& NodeMeshGeometryOperation::GetScalarB() const
	{
		return m_pD->m_pScalarB;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshGeometryOperation::SetScalarB(const NodeScalarPtr& pNode)
	{
		m_pD->m_pScalarB = pNode;
	}


	//---------------------------------------------------------------------------------------------
	const NodeMeshPtr& NodeMeshGeometryOperation::GetMeshA() const
	{
		return m_pD->m_pMeshA;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshGeometryOperation::SetMeshA(const NodeMeshPtr& pNode)
	{
		m_pD->m_pMeshA = pNode;
	}


	//---------------------------------------------------------------------------------------------
	const NodeMeshPtr& NodeMeshGeometryOperation::GetMeshB() const
	{
		return m_pD->m_pMeshB;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshGeometryOperation::SetMeshB(const NodeMeshPtr& pNode)
	{
		m_pD->m_pMeshB = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshGeometryOperation::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if (m_pMeshA)
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>(m_pMeshA->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}


