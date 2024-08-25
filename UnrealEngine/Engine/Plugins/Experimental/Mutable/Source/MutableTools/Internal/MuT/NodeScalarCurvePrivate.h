// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarCurve.h"
#include "MuT/NodeScalar.h"
#include "MuT/AST.h"

#include "MuR/SerialisationPrivate.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{
	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeScalarCurve::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr m_input_scalar;
		Curve m_curve;

		//!
		void Serialise(OutputArchive& arch) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_input_scalar;
			arch << m_curve;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
            uint32_t ver;
			arch >> ver;
            check(ver == 1);

			arch >> m_input_scalar;
			arch >> m_curve;
		}
	};


}
