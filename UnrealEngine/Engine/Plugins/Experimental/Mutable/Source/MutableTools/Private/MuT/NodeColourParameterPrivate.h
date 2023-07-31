// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourParameter.h"
#include "MuT/NodeColourPrivate.h"

#include "MuT/NodeRange.h"
#include "MuR/MutableMath.h"


namespace mu
{

	class NodeColourParameter::Private : public NodeColour::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		vec3<float> m_defaultValue;
		string m_name;
		string m_uid;

        TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 2;
			arch << ver;

			arch << m_defaultValue;
			arch << m_name;
            arch << m_uid;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==2);

			arch >> m_defaultValue;
			arch >> m_name;
            arch >> m_uid;
            arch >> m_ranges;
        }
	};

}
