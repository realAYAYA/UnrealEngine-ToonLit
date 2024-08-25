// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageInvert.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeImageInvert::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pBase;
		
		//!
		void Serialise(OutputArchive& arch) const
		{
			uint32_t ver = 0;
			arch << ver;

			arch << m_pBase;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
			uint32_t ver;
			arch >> ver;
			check(ver == 0);

			arch >> m_pBase;
		}
	};
}
