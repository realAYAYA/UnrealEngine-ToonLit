// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageMultiLayer.h"
#include "MuT/NodeColour.h"
#include "MuT/AST.h"


namespace mu
{

    class NodeImageMultiLayer::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pBase;
		NodeImagePtr m_pMask;
        NodeImagePtr m_pBlended;
        NodeRangePtr m_pRange;
		EBlendType m_type;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;
            arch << uint32_t(m_type);

			arch << m_pBase;
			arch << m_pMask;
            arch << m_pBlended;
            arch << m_pRange;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver<=1);

            uint32_t t;
            arch >> t;
			if (ver < 1)
			{
				++t;
			}
            m_type=(EBlendType)t;

            arch >> m_pBase;
			arch >> m_pMask;
            arch >> m_pBlended;
            arch >> m_pRange;

		}
	};

}
