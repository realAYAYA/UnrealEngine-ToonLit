// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeScalarPrivate.h"

#include "MuT/NodeScalarParameter.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeRange.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeScalarParameter::Private : public NodeScalar::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		float m_defaultValue = 0.0f;
		string m_name;
		string m_uid;
		PARAMETER_DETAILED_TYPE m_detailedType = PARAMETER_DETAILED_TYPE::UNKNOWN;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 5;
			arch << ver;

			arch << m_defaultValue;
			arch << m_name;
			arch << m_uid;
			arch << m_detailedType;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver>=4 && ver<=5);

			arch >> m_defaultValue;
			arch >> m_name;
            arch >> m_uid;
			if (ver < 5)
			{
				TArray<Ptr<NodeImage>> TempAdditionalImages;
				arch >> TempAdditionalImages;
			}
            arch >> m_detailedType;
            arch >> m_ranges;
        }
	};

}
