// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageMipmap.h"
#include "MuT/NodeScalar.h"
#include "MuT/AST.h"

#include "MuR/ImagePrivate.h"
#include "MuR/MemoryPrivate.h"


namespace mu
{	
    class NodeImageMipmap::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeImagePtr m_pSource;
		NodeScalarPtr m_pFactor;

		FMipmapGenerationSettings m_settings;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_pSource;
			arch << m_pFactor;

			arch << m_settings;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==1);

			arch >> m_pSource;
			arch >> m_pFactor;
			
			arch >> m_settings;
		}	
	};


}
