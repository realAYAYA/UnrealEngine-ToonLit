// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeProjector.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/Node.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static FNodeType s_nodeProjectorType = FNodeType( "NodeProjector", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeProjector::Serialise( const NodeProjector* p, OutputArchive& arch )
	{
        uint32 ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
	}

        
	//---------------------------------------------------------------------------------------------
	NodeProjectorPtr NodeProjector::StaticUnserialise( InputArchive& arch )
	{
        uint32 ver;
		arch >> ver;
		check( ver == 0 );

        uint32 id;
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
	const FNodeType* NodeProjector::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeProjector::GetStaticType()
	{
		return &s_nodeProjectorType;
	}


}


