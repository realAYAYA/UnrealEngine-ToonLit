// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeBool.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"

#include <stdint.h>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeBoolType = NODE_TYPE( "NodeBool", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeBool::Serialise( const NodeBool* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
	}

        
	//---------------------------------------------------------------------------------------------
	NodeBoolPtr NodeBool::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeBoolConstant::StaticUnserialise( arch ); break;
		case 1 :  return NodeBoolParameter::StaticUnserialise( arch ); break;
		case 2 :  return NodeBoolIsNull::StaticUnserialise( arch ); break;
		case 3 :  return NodeBoolNot::StaticUnserialise( arch ); break;
		case 4 :  return NodeBoolAnd::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeBool::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeBool::GetStaticType()
	{
		return &s_nodeBoolType;
	}


}


