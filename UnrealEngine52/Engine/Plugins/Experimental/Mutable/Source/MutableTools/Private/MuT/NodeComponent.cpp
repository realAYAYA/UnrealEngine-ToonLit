// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeComponent.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodeComponentNew.h"

#include <stdint.h>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeComponentType =
			NODE_TYPE( "NodeComponent", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeComponent::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeComponent::GetStaticType()
	{
		return &s_nodeComponentType;
	}


	//---------------------------------------------------------------------------------------------
	void NodeComponent::Serialise( const NodeComponent* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
	}

        
	//---------------------------------------------------------------------------------------------
	NodeComponentPtr NodeComponent::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
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


