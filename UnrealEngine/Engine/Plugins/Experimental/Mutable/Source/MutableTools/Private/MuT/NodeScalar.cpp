// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalar.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/Node.h"
#include "MuT/NodeScalarArithmeticOperation.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeScalarCurve.h"
#include "MuT/NodeScalarEnumParameter.h"
#include "MuT/NodeScalarParameter.h"
#include "MuT/NodeScalarSwitch.h"
#include "MuT/NodeScalarTable.h"
#include "MuT/NodeScalarVariation.h"

#include <stdint.h>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeScalarType = NODE_TYPE( "NodeScalar", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeScalar::Serialise( const NodeScalar* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
    }


    //---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeScalar::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeScalarConstant::StaticUnserialise( arch ); break;
		case 1 :  return NodeScalarParameter::StaticUnserialise( arch ); break;
		case 2 :  return NodeScalarEnumParameter::StaticUnserialise( arch ); break;
        case 3 :  return NodeScalarCurve::StaticUnserialise( arch ); break;
        case 4 :  return NodeScalarSwitch::StaticUnserialise( arch ); break;
        case 5 :  return NodeScalarArithmeticOperation::StaticUnserialise( arch ); break;
        case 6 :  return NodeScalarVariation::StaticUnserialise( arch ); break;
		case 7 :  return NodeScalarTable::StaticUnserialise( arch ); break;
        default : check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeScalar::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeScalar::GetStaticType()
	{
		return &s_nodeScalarType;
	}


}


