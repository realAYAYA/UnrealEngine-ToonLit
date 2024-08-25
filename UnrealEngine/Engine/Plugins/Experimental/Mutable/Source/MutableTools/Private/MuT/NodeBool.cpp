// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeBool.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static FNodeType s_nodeBoolType = FNodeType( "NodeBool", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeBool::Serialise( const NodeBool* p, OutputArchive& arch )
	{
        uint32 ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
	}

        
	//---------------------------------------------------------------------------------------------
	Ptr<NodeBool> NodeBool::StaticUnserialise( InputArchive& arch )
	{
        uint32 ver;
		arch >> ver;
		check( ver == 0 );

        uint32 id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeBoolConstant::StaticUnserialise( arch ); break;
		case 1 :  return NodeBoolParameter::StaticUnserialise( arch ); break;
		//case 2 :  return NodeBoolIsNull::StaticUnserialise( arch ); break;
		case 3 :  return NodeBoolNot::StaticUnserialise( arch ); break;
		case 4 :  return NodeBoolAnd::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeBool::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeBool::GetStaticType()
	{
		return &s_nodeBoolType;
	}


}


