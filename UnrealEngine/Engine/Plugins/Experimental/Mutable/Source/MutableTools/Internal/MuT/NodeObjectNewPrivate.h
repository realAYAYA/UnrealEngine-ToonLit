// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeObjectPrivate.h"

#include "MuT/NodeExtensionData.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeLOD.h"
#include "MuT/Compiler.h"


namespace mu
{


	class NodeObjectNew::Private : public NodeObject::Private
	{
	public:

		static FNodeType s_type;

		FString m_name;
		FString m_uid;

		TArray<NodeLODPtr> m_lods;

		TArray<NodeObjectPtr> m_children;

		struct NamedExtensionDataNode
		{
			Ptr<NodeExtensionData> Node;
			FString Name;

			void Serialise(OutputArchive& arch) const
			{
				arch << Node;
				arch << Name;
			}

			void Unserialise(InputArchive& arch)
			{
				arch >> Node;
				arch >> Name;
			}
		};

		TArray<NamedExtensionDataNode> m_extensionDataNodes;


		//! List of states
        TArray<FObjectState> m_states;


		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 4;
			arch << ver;

			arch << m_name;
			arch << m_uid;
			arch << m_lods;
			arch << m_children;
			arch << m_states;
			arch << m_extensionDataNodes;
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

			arch >> m_lods;
			arch >> m_children;
			arch >> m_states;

			if (ver >= 4)
			{
				arch >> m_extensionDataNodes;
			}
			else if (ver >= 3)
			{
				int32 Num = 0;
				arch >> Num;
				m_extensionDataNodes.SetNum(Num);
				for (int i=0;i<Num; ++i)
				{
					std::string Temp;
					arch >> Temp;
					m_extensionDataNodes[i].Name = Temp.c_str();
					arch >> m_extensionDataNodes[i].Node;
				}
			}
		}

        //! Return true if the given component is set in any lod of this object.
        bool HasComponent( const NodeComponent* pComponent ) const;

        // NodeObject::Private interface
        NodeLayoutPtr GetLayout( int lod, int component, int surface, int texture ) const override;

	};

}
