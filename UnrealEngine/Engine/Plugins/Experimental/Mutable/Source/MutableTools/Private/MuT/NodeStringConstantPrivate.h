// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeStringPrivate.h"

#include "MuT/NodeStringConstant.h"
#include "MuT/NodeImage.h"
#include "MuT/AST.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeStringConstant::Private : public NodeString::Private
	{
	public:

		static FNodeType s_type;

		FString m_value;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 1;
			arch << ver;

			arch << m_value;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver<=1);

			if (ver <= 0)
			{
				std::string Temp;
				arch >> Temp;
				m_value = Temp.c_str();
			}
			else
			{
				arch >> m_value;
			}
		}
	};


}
