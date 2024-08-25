// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeLayout.h"
#include "MuT/TablePrivate.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeMeshTable::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		FString ParameterName;
		TablePtr Table;
		FString ColumnName;
		bool bNoneOption = false;
		FString DefaultRowName;

		TArray<NodeLayoutPtr> Layouts;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 4;
			arch << ver;

			arch << ParameterName;
			arch << Table;
			arch << ColumnName;
			arch << Layouts;
			arch << bNoneOption;
			arch << DefaultRowName;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=1 && ver<=4);

			if (ver == 1)
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

			if (ver == 1)
			{
				std::string Temp;
				arch >> Temp;
				ColumnName = Temp.c_str();
			}
			else
			{
				arch >> ColumnName;
			}
			
			arch >> Layouts;

			if(ver >= 3)
			{
				arch >> bNoneOption;
			}
			else
			{
				bNoneOption = Table->GetPrivate()->bNoneOption_DEPRECATED;
			}

			if (ver >= 4)
			{
				arch >> DefaultRowName;
			}
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};

}
