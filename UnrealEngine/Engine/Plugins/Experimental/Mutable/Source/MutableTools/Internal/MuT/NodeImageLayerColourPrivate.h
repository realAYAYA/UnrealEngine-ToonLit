// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageLayerPrivate.h"
#include "MuT/NodeImageLayerColour.h"
#include "MuT/NodeColour.h"
#include "MuR/Image.h"


namespace mu
{

	class NodeImageLayerColour::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pBase;
		NodeImagePtr m_pMask;
		NodeColourPtr m_pColour;
		EBlendType m_type;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_pBase;
			arch << m_pMask;
			arch << m_pColour;
			arch << m_type;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver<=1);

			arch >> m_pBase;
			arch >> m_pMask;
			arch >> m_pColour;
			arch >> m_type;
			if (ver < 1)
			{
				m_type = EBlendType( uint32(m_type)+1 );
			}

		}
	};

}
