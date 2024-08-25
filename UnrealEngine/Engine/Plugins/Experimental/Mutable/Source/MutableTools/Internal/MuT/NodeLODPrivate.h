// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeComponentPrivate.h"
#include "MuT/NodeModifierPrivate.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeLOD::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		TArray<NodeComponentPtr> m_components;
		TArray<NodeModifierPtr> m_modifiers;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_components;
			arch << m_modifiers;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver == 1);

			arch >> m_components;
			arch >> m_modifiers;
		}
	};

}
