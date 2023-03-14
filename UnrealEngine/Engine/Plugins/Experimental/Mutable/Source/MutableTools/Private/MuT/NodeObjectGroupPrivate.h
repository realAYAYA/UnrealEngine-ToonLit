// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeObjectPrivate.h"

#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeLayout.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

	MUTABLE_DEFINE_ENUM_SERIALISABLE( NodeObjectGroup::CHILD_SELECTION )


	class NodeObjectGroup::Private : public NodeObject::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_name;
		string m_uid;

		CHILD_SELECTION m_type;

		//! Set the child selection type
		void SetSelectionType( CHILD_SELECTION );

		TArray<NodeObjectPtr> m_children;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_type;
			arch << m_name;
			arch << m_uid;
			arch << m_children;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==1);

			arch >> m_type;
			arch >> m_name;
            arch >> m_uid;
			arch >> m_children;
		}


        // NodeObject::Private interface
        NodeLayoutPtr GetLayout( int lod, int component, int surface, int texture ) const override;

	};

}
