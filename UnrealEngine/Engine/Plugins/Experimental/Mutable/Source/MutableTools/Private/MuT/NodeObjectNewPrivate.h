// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeObjectPrivate.h"
#include "MuT/NodeLODPrivate.h"
#include "MuT/CompilerPrivate.h"

#include "MuT/NodeExtensionData.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeLayout.h"


namespace mu
{


	class NodeObjectNew::Private : public NodeObject::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_name;
		string m_uid;

		TArray<NodeLODPtr> m_lods;

		TArray<NodeObjectPtr> m_children;

		struct NamedExtensionDataNode
		{
			NodeExtensionDataPtr Node;
			string Name;

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
            uint32_t ver = 3;
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
            check(ver >= 2);

			arch >> m_name;
            arch >> m_uid;
			arch >> m_lods;
			arch >> m_children;
			arch >> m_states;

			if (ver >= 3)
			{
				arch >> m_extensionDataNodes;
			}
		}

        //! Return true if the given component is set in any lod of this object.
        bool HasComponent( const NodeComponent* pComponent ) const;

        // NodeObject::Private interface
        NodeLayoutPtr GetLayout( int lod, int component, int surface, int texture ) const override;

	};

}
