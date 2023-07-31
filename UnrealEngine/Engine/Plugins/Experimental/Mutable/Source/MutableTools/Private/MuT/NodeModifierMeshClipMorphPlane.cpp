// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeModifierMeshClipMorphPlane.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeModifierMeshClipMorphPlanePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeModifierMeshClipMorphPlane::Private::s_type =
            NODE_TYPE( "ModifierMeshClipMorphPlane", NodeModifier::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeModifierMeshClipMorphPlane, EType::MeshClipMorphPlane, Node, Node::EType::Modifier)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeModifierMeshClipMorphPlane::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeModifierMeshClipMorphPlane::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    void NodeModifierMeshClipMorphPlane::SetInputNode( int i, NodePtr )
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeModifierMeshClipMorphPlane::SetPlane( float centerX, float centerY, float centerZ,
                                                   float normalX, float normalY, float normalZ)
	{
		m_pD->m_origin = vec3f(centerX, centerY, centerZ);
		m_pD->m_normal = vec3f(normalX, normalY, normalZ);
	}


	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipMorphPlane::SetParams(float dist, float factor)
	{
		m_pD->m_dist = dist;
		m_pD->m_factor = factor;
	}

	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipMorphPlane::SetMorphEllipse(float radius1, float radius2, float rotation)
	{
		m_pD->m_radius1 = radius1;
		m_pD->m_radius2 = radius2;
		m_pD->m_rotation = rotation;
	}

	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipMorphPlane::SetVertexSelectionBox(float centerX, float centerY, float centerZ, float radiusX, float radiusY, float radiusZ)
	{
		m_pD->m_vertexSelectionType = Private::VS_SHAPE;
		m_pD->m_selectionBoxOrigin = vec3f(centerX, centerY, centerZ);
		m_pD->m_selectionBoxRadius = vec3f(radiusX, radiusY, radiusZ);
	}

	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipMorphPlane::SetVertexSelectionBone(const char* strBoneName, float maxEffectRadius)
	{
		m_pD->m_vertexSelectionType = Private::VS_BONE_HIERARCHY;
		m_pD->m_vertexSelectionBone = strBoneName;
		m_pD->m_maxEffectRadius = maxEffectRadius;
	}

}

