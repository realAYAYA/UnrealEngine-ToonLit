// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeStringPrivate.h"

#include "MuT/NodeStringParameter.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeRange.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeStringParameter::Private : public NodeString::Private
	{
	public:

		static FNodeType s_type;

		FString m_defaultValue;
		FString m_name;
		FString m_uid;

        TArray<Ptr<NodeImage>> m_additionalImages;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 6;
			arch << ver;

			arch << m_defaultValue;
			arch << m_name;
			arch << m_uid;
			arch << m_additionalImages;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver>=4 && ver<=6);

			if (ver <= 5)
			{
				std::string Temp;
				arch >> Temp;
				m_defaultValue = Temp.c_str();
				arch >> Temp;
				m_name = Temp.c_str();
				arch >> Temp;
				m_uid = Temp.c_str();
			}
			else
			{
				arch >> m_defaultValue;
				arch >> m_name;
				arch >> m_uid;
			}

			arch >> m_additionalImages;
			if (ver <= 4)
			{
				int32 Dummy=0;
				arch >> Dummy;
			}
            arch >> m_ranges;
        }
	};

}
