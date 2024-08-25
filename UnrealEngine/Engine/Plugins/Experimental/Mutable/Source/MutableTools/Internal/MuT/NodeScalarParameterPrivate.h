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

		static FNodeType s_type;

		float m_defaultValue = 0.0f;
		FString m_name;
		FString m_uid;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 7;
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
            check(ver>=4 && ver<=7);

			arch >> m_defaultValue;
			if (ver <= 6)
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

			if (ver < 5)
			{
				TArray<Ptr<NodeImage>> TempAdditionalImages;
				arch >> TempAdditionalImages;
			}

			if (ver <= 5)
			{
				int32 Dummy=0;
				arch >> Dummy;
			}

            arch >> m_ranges;
        }
	};

}
