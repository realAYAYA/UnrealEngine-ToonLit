// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColour.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeColourArithmeticOperation.h"
#include "MuT/NodeColourConstant.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeColourParameter.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeColourSwitch.h"
#include "MuT/NodeColourTable.h"
#include "MuT/NodeColourVariation.h"

#include <stdint.h>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeColourType = NODE_TYPE( "NodeColour", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeColour::Serialise( const NodeColour* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
    }

        
	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeColour::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeColourConstant::StaticUnserialise( arch ); break;
		case 1 :  return NodeColourParameter::StaticUnserialise( arch ); break;
		case 2 :  return NodeColourSampleImage::StaticUnserialise( arch ); break;
		case 3 :  return NodeColourTable::StaticUnserialise( arch ); break;
		//case 4 :  return NodeColourImageSize::StaticUnserialise( arch ); break; // deprecated
		case 5 :  return NodeColourFromScalars::StaticUnserialise( arch ); break;
        case 6 :  return NodeColourArithmeticOperation::StaticUnserialise( arch ); break;
        case 7 :  return NodeColourSwitch::StaticUnserialise( arch ); break;
        case 8 :  return NodeColourVariation::StaticUnserialise( arch ); break;
        default : check(false);
		}

		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeColour::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeColour::GetStaticType()
	{
		return &s_nodeColourType;
	}


}


