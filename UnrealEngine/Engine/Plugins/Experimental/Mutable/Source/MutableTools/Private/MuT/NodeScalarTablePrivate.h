// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarTable.h"
#include "MuT/TablePrivate.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeScalarTable::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		FString ParameterName;
		TablePtr Table;
		FString ColumnName;
		bool bNoneOption = false;
		FString DefaultRowName;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 3;
			arch << ver;

			arch << ParameterName;
			arch << Table;
			arch << ColumnName;
			arch << bNoneOption;
			arch << DefaultRowName;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver <= 3);

			if (ver==0)
			{
				std::string Temp;
				arch >> Temp;
				ParameterName = Temp.c_str();
			}
			else
			{
				arch >> ParameterName;
			}

			arch >> Table;

			if (ver == 0)
			{
				std::string Temp;
				arch >> Temp;
				ColumnName = Temp.c_str();
			}
			else
			{
				arch >> ColumnName;
			}

			if(ver >= 2)
			{
				arch >> bNoneOption;
			}
			else
			{
				bNoneOption = Table->GetPrivate()->bNoneOption_DEPRECATED;
			}

			if (ver >= 3)
			{
				arch >> DefaultRowName;
			}
		}

	};

}
