// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodeObjectPrivate.h"
#include "MuT/NodeObjectState.h"
#include "MuT/CompilerPrivate.h"


namespace mu
{

	class NodeObjectState::Private : public NodeObject::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:


        // NodeObject::Private interface
        NodeLayoutPtr GetLayout( int lod, int component, int surface, int texture ) const override;


		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------
		static NODE_TYPE s_type;

        OBJECT_STATE m_state;

		NodeObjectPtr m_pSource;
		NodeObjectPtr m_pRoot;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_state;
			arch << m_pSource;
			arch << m_pRoot;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_state;
			arch >> m_pSource;
			arch >> m_pRoot;
		}
	};

}

