// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeRangePrivate.h"
#include "MuT/NodeRangeFromScalar.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalar.h"


namespace mu
{


    class NodeRangeFromScalar::Private : public NodeRange::Private
	{
	public:

		static FNodeType s_type;

        NodeScalarPtr m_pSize;
        FString m_name;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

            arch << m_name;
            arch << m_pSize;
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
				m_name = Temp.c_str();
			}
			else
			{
				arch >> m_name;
			}

            arch >> m_pSize;
		}
	};


}
