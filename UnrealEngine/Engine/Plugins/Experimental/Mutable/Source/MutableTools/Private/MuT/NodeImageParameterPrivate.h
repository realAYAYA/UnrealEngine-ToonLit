// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImageParameter.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeRange.h"


namespace mu
{

    class NodeImageParameter::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

    	FName m_defaultValue;
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
            check(ver <= 4);

			if (ver >= 3)
			{
				arch >> m_defaultValue;
			}
			
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

			if (ver >= 2)
			{
				arch >> m_ranges;
			}
		}
	};

}
