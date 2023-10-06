// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuR/ImagePrivate.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageTransform.h"
#include "MuT/AST.h"
#include "MuT/NodeScalar.h"

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

		EAddressMode AddressMode = EAddressMode::Wrap;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 1;
			arch << ver;

			arch << m_pBase;
			arch << m_pOffsetX;
			arch << m_pOffsetY;
			arch << m_pScaleX;
			arch << m_pScaleY;
			arch << m_pRotation;
			arch << static_cast<uint32>(AddressMode);
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver <= 1);

			arch >> m_pBase;
			arch >> m_pOffsetX;
			arch >> m_pOffsetY;
			arch >> m_pScaleX;
			arch >> m_pScaleY;
			arch >> m_pRotation;

			uint32 AddressModeValue = static_cast<uint32>(EAddressMode::Wrap);
			if (ver == 1)
			{
				arch >> AddressModeValue;
			}

			AddressMode = static_cast<EAddressMode>(AddressModeValue);
		}
	};


}
