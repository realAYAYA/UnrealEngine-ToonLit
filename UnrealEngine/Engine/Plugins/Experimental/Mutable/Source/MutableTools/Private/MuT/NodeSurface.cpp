// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeSurface.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/NodeSurfaceVariation.h"

#include <stdint.h>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    static NODE_TYPE s_nodeSurfaceType =
            NODE_TYPE( "NodeSurface", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
    const NODE_TYPE* NodeSurface::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
    const NODE_TYPE* NodeSurface::GetStaticType()
	{
        return &s_nodeSurfaceType;
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurface::Serialise( const NodeSurface* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
	}


    //---------------------------------------------------------------------------------------------
    NodeSurfacePtr NodeSurface::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
        case 0: return NodeSurfaceNew::StaticUnserialise( arch ); break;
        case 1: return NodeSurfaceEdit::StaticUnserialise( arch ); break;
		//case 2: return NodeSelectSurface::StaticUnserialise(arch); break;
		case 3: return NodeSurfaceVariation::StaticUnserialise(arch); break;
		default : check(false);
		}

		return 0;
	}

}


