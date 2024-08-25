// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/AST.h"

#include "MuR/MutableMath.h"


namespace mu
{
    class NodeModifierMeshClipMorphPlane::Private : public NodeModifier::Private
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

		// Max distance a vertex can have to the bone in order to be affected. A negative value
		// means no limit.
		float m_maxEffectRadius;

        //!
        void Serialise( OutputArchive& arch ) const
        {
            NodeModifier::Private::Serialise(arch);

			uint32 ver = 3;
            arch << ver;

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
            arch << m_vertexSelectionBone;
			arch << m_maxEffectRadius;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            NodeModifier::Private::Unserialise( arch );

            uint32 ver;
            arch >> ver;
            check(ver>=2);

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
			if (ver >= 3)
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

    };

}
