// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshClipMorphPlane.h"
#include "MuT/AST.h"

#include "MuR/MutableMath.h"


namespace mu
{


    class NodeMeshClipMorphPlane::Private : public NodeMesh::Private
	{
	public:

		Private()
			: m_dist(0.0f), m_factor(0.0f)
			, m_radius1(0.0f), m_radius2(0.0f), m_rotation(0.0f)
			, m_vertexSelectionType(VS_ALL)
			, m_maxEffectRadius(-1.f)
		{
		}

		static FNodeType s_type;

		NodeMeshPtr m_pSource;

		// Morph field parameters

		//! Distance to the plane of last affected vertex
		float m_dist;

		//! "Linearity" factor of the influence.
		float m_factor;

		// Ellipse location
		FVector3f m_origin;
		FVector3f m_normal;
		float m_radius1, m_radius2, m_rotation;

		//! Typed of vertex selection
		typedef enum
		{
			//! All vertices, so no extra info is needed
			VS_ALL,

			//! Select vertices inside a shape
			VS_SHAPE,

			//! Select all vertices affected by any bone in a sub hierarchy
			VS_BONE_HIERARCHY,

		} VERTEX_SELECTION;

		// Vertex selection box 
        uint8 m_vertexSelectionType;
		FVector3f m_selectionBoxOrigin;
		FVector3f m_selectionBoxRadius;
		uint16 m_vertexSelectionBone;

		TArray<FString> m_tags;

		// Max distance a vertex can have to the bone in order to be affected. A negative value
		// means no limit.
		float m_maxEffectRadius;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 5;
			arch << ver;

			arch << m_pSource;
			arch << m_origin;
			arch << m_normal;
			arch << m_dist;
			arch << m_factor;
			arch << m_radius1;
			arch << m_radius2;
			arch << m_rotation;
			arch << m_vertexSelectionType;
			arch << m_selectionBoxOrigin;
			arch << m_selectionBoxRadius;
			arch << m_tags;
			arch << m_vertexSelectionBone;
			arch << m_maxEffectRadius;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=3&&ver<=5);

			arch >> m_pSource;
			arch >> m_origin;
			arch >> m_normal;
			arch >> m_dist;
			arch >> m_factor;
			arch >> m_radius1;
			arch >> m_radius2;
			arch >> m_rotation;
			arch >> m_vertexSelectionType;
			arch >> m_selectionBoxOrigin;
			arch >> m_selectionBoxRadius;

			if (ver >= 5)
			{
				arch >> m_tags;
			}
			else
			{
				TArray<std::string> Temp;
				arch >> Temp;
				m_tags.SetNum(Temp.Num());
				for (int32 i=0;i<Temp.Num();++i)
				{
					m_tags[i] = Temp[i].c_str();
				}
			}

			if (ver >= 4)
			{
				arch >> m_vertexSelectionBone;
			}
			else
			{
				string OldVertexSelectionBone;
				arch >> OldVertexSelectionBone;
				m_vertexSelectionBone = 0;
			}
			arch >> m_maxEffectRadius;
		}

        // NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};


}
