// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/AST.h"
#include "MuR/ImagePrivate.h"


namespace mu
{


	class NodeImageFormat::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		EImageFormat m_format = EImageFormat::IF_NONE;
		EImageFormat m_formatIfAlpha = EImageFormat::IF_NONE;
        NodeImagePtr m_source;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

            arch << (uint32)m_format;
            arch << (uint32)m_formatIfAlpha;
            arch << m_source;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==1);

			uint32 format;
			arch >> format;
			m_format = EImageFormat(format);

			arch >> format;
			m_formatIfAlpha = EImageFormat(format);

			arch >> m_source;
		}
	};


}
