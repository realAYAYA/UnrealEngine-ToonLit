// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeProjector.h"

#include "MuT/NodeRange.h"
#include "MuR/MutableMath.h"


namespace mu
{


	class NodeProjector::Private : public Node::Private
	{
	};


	class NodeProjectorConstant::Private : public NodeProjector::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

        PROJECTOR_TYPE m_type = PROJECTOR_TYPE::PLANAR;
		vec3<float> m_position;
		vec3<float> m_direction;
		vec3<float> m_up;
        vec3<float> m_scale;
        float m_projectionAngle = 0.0f;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 2;
			arch << ver;

            arch << m_type;
            arch << m_position;
            arch << m_direction;
			arch << m_up;
			arch << m_scale;
            arch << m_projectionAngle;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==2);

            arch >> m_type;
			arch >> m_position;
			arch >> m_direction;
			arch >> m_up;
            arch >> m_scale;
            arch >> m_projectionAngle;
        }

	};


	class NodeProjectorParameter::Private : public NodeProjectorConstant::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_name;
		string m_uid;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
			NodeProjectorConstant::Private::Serialise( arch );

            uint32_t ver = 2;
			arch << ver;

			arch << m_name;
            arch << m_uid;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
			NodeProjectorConstant::Private::Unserialise( arch );

            uint32_t ver;
			arch >> ver;
            check(ver<=2);

			arch >> m_name;
            if (ver>=1)
            {
                arch >> m_uid;
            }

            if (ver>=2)
            {
                arch >> m_ranges;
            }
        }
	};


}
