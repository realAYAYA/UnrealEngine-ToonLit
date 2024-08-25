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
	FNodeType NodeMeshMorph::Private::s_type = FNodeType( "MeshMorph", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshMorph, EType::Morph, Node, Node::EType::Mesh)


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
			NodeMesh::Private* pPrivate = static_cast<NodeMesh::Private*>( Base->GetBasePrivate() );
			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}


