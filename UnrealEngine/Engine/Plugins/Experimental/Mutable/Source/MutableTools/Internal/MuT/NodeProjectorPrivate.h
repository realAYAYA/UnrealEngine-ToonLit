// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeProjector.h"

#include "MuT/NodeRange.h"
#include "MuR/MutableMath.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeProjector::Private : public Node::Private
	{
	};


	class NodeProjectorConstant::Private : public NodeProjector::Private
	{
	public:

		static FNodeType s_type;

        PROJECTOR_TYPE m_type = PROJECTOR_TYPE::PLANAR;
		FVector3f m_position;
		FVector3f m_direction;
		FVector3f m_up;
		FVector3f m_scale;
        float m_projectionAngle = 0.0f;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 2;
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
            uint32 ver;
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

		static FNodeType s_type;

		FString m_name;
		FString m_uid;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
			NodeProjectorConstant::Private::Serialise( arch );

            uint32 ver = 3;
			arch << ver;

			arch << m_name;
            arch << m_uid;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
			NodeProjectorConstant::Private::Unserialise( arch );

            uint32 ver;
			arch >> ver;
            check(ver<=3);

			if (ver <= 2)
			{
				std::string Temp;
				arch >> Temp;
				m_name = Temp.c_str();
				if (ver >= 1)
				{
					arch >> Temp;
					m_uid = Temp.c_str();
				}
			}
			else
			{
				arch >> m_name;
				arch >> m_uid;
			}

            if (ver>=2)
            {
                arch >> m_ranges;
            }
        }
	};


}
