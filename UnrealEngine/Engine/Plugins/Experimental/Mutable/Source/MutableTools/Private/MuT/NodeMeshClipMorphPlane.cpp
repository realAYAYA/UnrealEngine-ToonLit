// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeMeshClipMorphPlane.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMeshClipMorphPlanePrivate.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>



namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeMeshClipMorphPlane::Private::s_type =
            NODE_TYPE( "MeshClipMorphPlane", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshClipMorphPlane, EType::ClipMorphPlane, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeMeshClipMorphPlane::GetInputCount() const
	{
		return 1;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeMeshClipMorphPlane::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
        return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshClipMorphPlane::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());
		if (i==0)
		{
			m_pD->m_pSource = dynamic_cast<NodeMesh*>( pNode.get() );
		}
	}


	//---------------------------------------------------------------------------------------------
    const char* NodeMeshClipMorphPlane::GetInputName( int i ) const
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
        return "Source";
	}


	//---------------------------------------------------------------------------------------------
    const NODE_TYPE* NodeMeshClipMorphPlane::GetInputType( int i ) const
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
        return NodeMesh::GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshClipMorphPlane::GetSource() const
	{
		return m_pD->m_pSource;
	}


    //---------------------------------------------------------------------------------------------
    void NodeMeshClipMorphPlane::SetSource( NodeMesh* p )
    {
        m_pD->m_pSource = p;
    }


	//---------------------------------------------------------------------------------------------
	void NodeMeshClipMorphPlane::SetPlane(float centerX, float centerY, float centerZ, float normalX, float normalY, float normalZ)
	{
		m_pD->m_origin = vec3f(centerX, centerY, centerZ);
		m_pD->m_normal = vec3f(normalX, normalY, normalZ);
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshClipMorphPlane::SetParams(float dist, float factor)
	{
		m_pD->m_dist = dist;
		m_pD->m_factor = factor;
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshClipMorphPlane::SetMorphEllipse(float radius1, float radius2, float rotation)
	{
		m_pD->m_radius1 = radius1;
		m_pD->m_radius2 = radius2;
		m_pD->m_rotation = rotation;
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshClipMorphPlane::SetVertexSelectionBox(float centerX, float centerY, float centerZ, float radiusX, float radiusY, float radiusZ)
	{
		m_pD->m_vertexSelectionType = Private::VS_SHAPE;
		m_pD->m_selectionBoxOrigin = vec3f(centerX, centerY, centerZ);
		m_pD->m_selectionBoxRadius = vec3f(radiusX, radiusY, radiusZ);
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshClipMorphPlane::SetVertexSelectionBone(const char* strBoneName, float maxEffectRadius)
	{
		m_pD->m_vertexSelectionType = Private::VS_BONE_HIERARCHY;
		m_pD->m_vertexSelectionBone = strBoneName;
		m_pD->m_maxEffectRadius = maxEffectRadius;
	}

	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeMeshClipMorphPlane::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if ( m_pSource )
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_pSource->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}

	//---------------------------------------------------------------------------------------------
	void NodeMeshClipMorphPlane::AddTag(const char* tagName)
	{
		m_pD->m_tags.Add(tagName);
	}
}
