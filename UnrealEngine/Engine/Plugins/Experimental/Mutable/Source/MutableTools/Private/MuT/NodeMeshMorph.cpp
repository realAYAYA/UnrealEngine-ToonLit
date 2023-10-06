// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshMorph.h"

#include "Containers/Array.h"
#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshMorphPrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshMorph::Private::s_type = NODE_TYPE( "MeshMorph", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshMorph, EType::Morph, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshMorph::GetInputCount() const
	{
		return 3;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshMorph::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		switch (i)
		{
		case 0:
			pResult = m_pD->Factor.get();
			break;

		case 1:
			pResult = m_pD->Base.get();
			break;

		case 2:
			pResult = m_pD->Morph.get();
			break;

		default:
			check(false);
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshMorph::SetInputNode( int i, Ptr<Node> pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->Factor = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		case 1:
			m_pD->Base = dynamic_cast<NodeMesh*>(pNode.get());
			break;

		case 2:
			m_pD->Morph = dynamic_cast<NodeMesh*>(pNode.get());
			break;

		default:
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	Ptr<NodeScalar> NodeMeshMorph::GetFactor() const
	{
		return m_pD->Factor.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshMorph::SetFactor( Ptr<NodeScalar> pNode )
	{
		m_pD->Factor = pNode;
	}


	//---------------------------------------------------------------------------------------------
	Ptr<NodeMesh> NodeMeshMorph::GetBase() const
	{
		return m_pD->Base.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshMorph::SetBase( Ptr<NodeMesh> pNode )
	{
		m_pD->Base = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMeshMorph::GetMorph() const
	{
		return m_pD->Morph.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshMorph::SetMorph( Ptr<NodeMesh> pNode )
	{
		m_pD->Morph = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshMorph::SetReshapeSkeleton(bool bEnable)
	{
		m_pD->bReshapeSkeleton = bEnable;
	}	

	//---------------------------------------------------------------------------------------------
	void NodeMeshMorph::AddBoneToDeform(const uint16 BoneId)
	{
		m_pD->BonesToDeform.Emplace(BoneId);
	}

	void NodeMeshMorph::AddPhysicsBodyToDeform(const uint16 BoneId)
	{
		m_pD->PhysicsToDeform.Emplace(BoneId);
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshMorph::SetReshapePhysicsVolumes(bool bEnable)
	{
		m_pD->bReshapePhysicsVolumes = bEnable;
	}

	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshMorph::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if ( Base )
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( Base->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}


