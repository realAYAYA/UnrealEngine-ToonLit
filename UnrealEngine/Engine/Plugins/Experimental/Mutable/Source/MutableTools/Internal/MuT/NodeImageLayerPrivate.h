// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageLayer.h"
#include "MuT/NodeColour.h"
#include "MuT/AST.h"


namespace mu
{

	class NodeImageLayer::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pBase;
		NodeImagePtr m_pMask;
		NodeImagePtr m_pBlended;
		EBlendType m_type;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_pBase;
			arch << m_pMask;
			arch << m_pBlended;
            arch << (uint32_t)m_type;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver<=1);

			arch >> m_pBase;
			arch >> m_pMask;
			arch >> m_pBlended;

            uint32_t t;
            arch >> t;
			if (ver < 1)
			{
				++t;
			}
            m_type=(EBlendType)t;
		}
	};

}
