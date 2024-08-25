// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeScalarPrivate.h"

#include "MuT/NodeScalarEnumParameter.h"
#include "MuT/NodeImage.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeScalarEnumParameter::Private : public NodeScalar::Private
	{
	public:

		static FNodeType s_type;

		FString m_name;
		FString m_uid;
		int32 m_defaultValue = 0;

		struct OPTION
		{
			FString name;
			float value;

			//!
			void Serialise( OutputArchive& arch ) const
			{
                uint32 ver = 1;
				arch << ver;

				arch << name;
				arch << value;
			}

			//!
			void Unserialise( InputArchive& arch )
			{
                uint32 ver;
				arch >> ver;
                check(ver<=1);

				if (ver == 0)
				{
					std::string Temp;
					arch >> Temp;
					name = Temp.c_str();
				}
				else
				{
					arch >> name;
				}
				arch >> value;
			}
		};

		TArray<OPTION> m_options;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 4;
			arch << ver;

			arch << m_name;
			arch << m_uid;
			arch << m_defaultValue;
            arch << m_options;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver>=2 && ver<=4);

			if (ver <= 3)
			{
				std::string Temp;
				arch >> Temp;
				m_name = Temp.c_str();
				arch >> Temp;
				m_uid = Temp.c_str();
			}
			else
			{
				arch >> m_name;
				arch >> m_uid;
			}

			arch >> m_defaultValue;
			if (ver <= 2)
			{
				int32 Dummy = 0;
				arch >> Dummy;
			}
			arch >> m_options;
            arch >> m_ranges;
        }
	};

}

