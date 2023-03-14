// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourPrivate.h"
#include "MuT/NodeColourArithmeticOperation.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

	MUTABLE_DEFINE_ENUM_SERIALISABLE(NodeColourArithmeticOperation::OPERATION)

	class NodeColourArithmeticOperation::Private : public NodeColour::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeColourArithmeticOperation::OPERATION m_operation;
		NodeColourPtr m_pA;
		NodeColourPtr m_pB;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_operation;
			arch << m_pA;
			arch << m_pB;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_operation;
			arch >> m_pA;
			arch >> m_pB;
		}
	};


}
