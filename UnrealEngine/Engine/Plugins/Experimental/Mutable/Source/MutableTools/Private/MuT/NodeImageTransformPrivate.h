// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageTransform.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{


	class NodeImageTransform::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

		NodeImagePtr m_pBase;
		NodeScalarPtr m_pOffsetX;
		NodeScalarPtr m_pOffsetY;
		NodeScalarPtr m_pScaleX;
		NodeScalarPtr m_pScaleY;
		NodeScalarPtr m_pRotation;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pBase;
			arch << m_pOffsetX;
			arch << m_pOffsetY;
			arch << m_pScaleX;
			arch << m_pScaleY;
			arch << m_pRotation;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pBase;
			arch >> m_pOffsetX;
			arch >> m_pOffsetY;
			arch >> m_pScaleX;
			arch >> m_pScaleY;
			arch >> m_pRotation;
		}
	};


}
