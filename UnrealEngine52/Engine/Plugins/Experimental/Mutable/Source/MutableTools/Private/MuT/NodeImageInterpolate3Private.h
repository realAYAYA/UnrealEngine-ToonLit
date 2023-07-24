// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImageInterpolate3.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeImageInterpolate3::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeScalarPtr m_pFactor1;
		NodeScalarPtr m_pFactor2;
		NodeImagePtr m_pTarget0;
		NodeImagePtr m_pTarget1;
		NodeImagePtr m_pTarget2;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pFactor1;
			arch << m_pFactor2;
			arch << m_pTarget0;
			arch << m_pTarget1;
			arch << m_pTarget2;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pFactor1;
			arch >> m_pFactor2;
			arch >> m_pTarget0;
			arch >> m_pTarget1;
			arch >> m_pTarget2;
		}
	};


}
