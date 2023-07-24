// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeProjector.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/Node.h"

#include <stdint.h>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeProjectorType = NODE_TYPE( "NodeProjector", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeProjector::Serialise( const NodeProjector* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
	}

        
	//---------------------------------------------------------------------------------------------
	NodeProjectorPtr NodeProjector::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeProjectorConstant::StaticUnserialise( arch ); break;
		case 1 :  return NodeProjectorParameter::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeProjector::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeProjector::GetStaticType()
	{
		return &s_nodeProjectorType;
	}


}


