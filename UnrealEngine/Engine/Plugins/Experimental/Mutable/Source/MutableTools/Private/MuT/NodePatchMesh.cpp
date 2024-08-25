// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodePatchMesh.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMeshPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodePatchMesh::Private::s_type =
			FNodeType( "MeshPatch", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodePatchMesh, EType::PatchMesh, Node, Node::EType::None);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMesh* NodePatchMesh::GetRemove() const
	{
        return m_pD->m_pRemove.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodePatchMesh::SetRemove( NodeMesh* pRemove )
	{
		m_pD->m_pRemove = pRemove;
	}


	//---------------------------------------------------------------------------------------------
    NodeMesh* NodePatchMesh::GetAdd() const
	{
        return m_pD->m_pAdd.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodePatchMesh::SetAdd( NodeMesh* pAdd )
	{
		m_pD->m_pAdd = pAdd;
	}



}


