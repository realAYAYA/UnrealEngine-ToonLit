// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeObjectPrivate.h"

#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeLayout.h"


namespace mu
{
	MUTABLE_DEFINE_ENUM_SERIALISABLE(NodeObjectGroup::CHILD_SELECTION)


	class NodeObjectGroup::Private : public NodeObject::Private
	{
	public:

		static FNodeType s_type;

		FString Name;
		FString Uid;

		CHILD_SELECTION m_type;

		//! Set the child selection type
		void SetSelectionType( CHILD_SELECTION );

		TArray<NodeObjectPtr> m_children;
		int32 DefaultValue;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 3;
			arch << ver;

			arch << m_type;
			arch << Name;
			arch << Uid;
			arch << m_children;
			arch << DefaultValue;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver>=1 && ver<=3);

			arch >> m_type;
			if (ver <= 1)
			{
				std::string Temp;
				arch >> Temp;
				Name = Temp.c_str();
				arch >> Temp;
				Uid = Temp.c_str();
			}
			else
			{
				arch >> Name;
				arch >> Uid;
			}

			arch >> m_children;

			if (ver >= 3)
			{
				arch >> DefaultValue;
			}
		}


        // NodeObject::Private interface
        NodeLayoutPtr GetLayout( int lod, int component, int surface, int texture ) const override;

	};

}
