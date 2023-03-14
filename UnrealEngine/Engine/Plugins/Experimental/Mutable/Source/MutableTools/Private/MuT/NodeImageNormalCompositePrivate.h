// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageNormalComposite.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

	class NodeImageNormalComposite::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeImagePtr m_pBase;
		NodeImagePtr m_pNormal;
		float m_power;
		ECompositeImageMode m_mode;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pBase;
			arch << m_pNormal;
			arch << m_power;
			arch << m_mode;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pBase;
			arch >> m_pNormal;
			arch >> m_power;
			arch >> m_mode;
		}
	};

}
