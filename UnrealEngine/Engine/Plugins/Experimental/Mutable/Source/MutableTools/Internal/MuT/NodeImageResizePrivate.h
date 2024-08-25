// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageResize.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeImageResize::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pBase;
		bool m_relative = true;
		float m_sizeX = 0.5f, m_sizeY = 0.5f;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pBase;
			arch << m_relative;
			arch << m_sizeX;
			arch << m_sizeY;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pBase;
			arch >> m_relative;
			arch >> m_sizeX;
			arch >> m_sizeY;
		}
	};


}
