// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeModifierMeshClipDeform.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Mesh.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeModifierMeshClipDeformPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeModifierMeshClipDeform::Private::s_type =
			FNodeType( "NodeModifierMeshClipDeform", NodeModifier::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeModifierMeshClipDeform, EType::MeshClipDeform, Node, Node::EType::Modifier)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipDeform::SetClipMesh(NodeMesh* ClipMesh)
	{
		m_pD->ClipMesh = ClipMesh;
	}

	void NodeModifierMeshClipDeform::SetBindingMethod(EShapeBindingMethod BindingMethod)
	{
		check(BindingMethod != EShapeBindingMethod::ReshapeClosestProject);
		m_pD->BindingMethod = BindingMethod;
	}

}
