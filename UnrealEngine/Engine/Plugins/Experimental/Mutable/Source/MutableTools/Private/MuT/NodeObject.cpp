// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeObject.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeObjectState.h"

#include <stdint.h>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE s_nodeObjectType = NODE_TYPE( "NodeObject", NodeObject::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeObject::Serialise( const NodeObject* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
    }

    //---------------------------------------------------------------------------------------------
	NodeObjectPtr NodeObject::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeObjectNew::StaticUnserialise( arch ); break;
		case 1 :  return NodeObjectGroup::StaticUnserialise( arch ); break;
        case 2 :  return NodeObjectState::StaticUnserialise( arch ); break;
        //case 3 :  return NodeObjectTransform::StaticUnserialise( arch ); break;
        default : check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeObject::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeObject::GetStaticType()
	{
		return &s_nodeObjectType;
	}


}


