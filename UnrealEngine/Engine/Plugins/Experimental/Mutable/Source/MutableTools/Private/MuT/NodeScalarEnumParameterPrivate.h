// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeScalarPrivate.h"

#include "MuT/NodeScalarEnumParameter.h"
#include "MuT/NodeImage.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeScalarEnumParameter::Private : public NodeScalar::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_name;
		string m_uid;
		int m_defaultValue = 0;
		PARAMETER_DETAILED_TYPE m_detailedType = PARAMETER_DETAILED_TYPE::UNKNOWN;

		struct OPTION
		{
			string name;
			float value;

			//!
			void Serialise( OutputArchive& arch ) const
			{
                uint32_t ver = 0;
				arch << ver;

				arch << name;
				arch << value;
			}

			//!
			void Unserialise( InputArchive& arch )
			{
                uint32_t ver;
				arch >> ver;
                check(ver<=0);

				arch >> name;
				arch >> value;
			}
		};

		TArray<OPTION> m_options;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 2;
			arch << ver;

			arch << m_name;
			arch << m_uid;
			arch << m_defaultValue;
			arch << m_detailedType;
            arch << m_options;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==2);

			arch >> m_name;
            arch >> m_uid;
			arch >> m_defaultValue;
			arch >> m_detailedType;
			arch >> m_options;
            arch >> m_ranges;
        }
	};

}

