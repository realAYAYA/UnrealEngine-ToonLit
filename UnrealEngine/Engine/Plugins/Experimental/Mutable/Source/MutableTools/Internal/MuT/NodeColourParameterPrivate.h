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

		static FNodeType s_type;

		FVector4f m_defaultValue;
		FString m_name;
		FString m_uid;

        TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 4;
			arch << ver;

			arch << m_defaultValue;
			arch << m_name;
            arch << m_uid;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=2&&ver<=4);

			if (ver <= 3)
			{
				FVector3f TempDefaultValue;
				arch >> TempDefaultValue;

				m_defaultValue = TempDefaultValue;
				m_defaultValue[3] = 1.f;
			}
			else
			{
				arch >> m_defaultValue;
			}
			
			if (ver <= 2)
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
			
			arch >> m_ranges;
        }
	};

}
