// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeComponent.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodeComponentNew.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static FNodeType s_nodeComponentType = FNodeType( "NodeComponent", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeComponent::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeComponent::GetStaticType()
	{
		return &s_nodeComponentType;
	}


	//---------------------------------------------------------------------------------------------
	void NodeComponent::Serialise( const NodeComponent* p, OutputArchive& arch )
	{
        uint32 ver = 0;
		arch << ver;

		arch << uint32(p->Type);
		p->SerialiseWrapper(arch);
	}

        
	//---------------------------------------------------------------------------------------------
	NodeComponentPtr NodeComponent::StaticUnserialise( InputArchive& arch )
	{
        uint32 ver;
		arch >> ver;
		check( ver == 0 );

        uint32 id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeComponentNew::StaticUnserialise( arch ); break;
		case 1 :  return NodeComponentEdit::StaticUnserialise( arch ); break;
//		case 2 :  return NodeSelectComponent::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}

}


