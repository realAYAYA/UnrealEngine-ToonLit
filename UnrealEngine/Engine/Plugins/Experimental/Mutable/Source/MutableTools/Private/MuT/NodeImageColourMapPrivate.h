// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageColourMap.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{


	class NodeImageColourMap::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeImagePtr m_pBase;
		NodeImagePtr m_pMask;
		NodeImagePtr m_pMap;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pBase;
			arch << m_pMask;
			arch << m_pMap;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pBase;
			arch >> m_pMask;
			arch >> m_pMap;
		}
	};


}

