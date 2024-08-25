// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeMesh.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeMeshApplyPose.h"
#include "MuT/NodeMeshClipDeform.h"
#include "MuT/NodeMeshClipMorphPlane.h"
#include "MuT/NodeMeshClipWithMesh.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshInterpolate.h"
#include "MuT/NodeMeshMakeMorph.h"
#include "MuT/NodeMeshMorph.h"
#include "MuT/NodeMeshReshape.h"
#include "MuT/NodeMeshSwitch.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeMeshTangents.h"
#include "MuT/NodeMeshTransform.h"
#include "MuT/NodeMeshVariation.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static FNodeType s_nodeMeshType = FNodeType( "NodeMesh", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeMesh::Serialise( const NodeMesh* p, OutputArchive& arch )
	{
        uint32 ver = 0;
		arch << ver;

		arch << uint32(p->Type);
		p->SerialiseWrapper( arch );
    }


    //---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMesh::StaticUnserialise( InputArchive& arch )
	{
        uint32 ver;
		arch >> ver;
		check( ver == 0 );

        uint32 id;
		arch >> id;

		switch (EType(id))
		{
		case EType::Constant:		return NodeMeshConstant::StaticUnserialise( arch ); break;
		case EType::Interpolate:	return NodeMeshInterpolate::StaticUnserialise( arch ); break;
		case EType::Table:			return NodeMeshTable::StaticUnserialise( arch ); break;
		case EType::Format:			return NodeMeshFormat::StaticUnserialise( arch ); break;
		case EType::Tangents:		return NodeMeshTangents::StaticUnserialise( arch ); break;
		case EType::Morph:			return NodeMeshMorph::StaticUnserialise( arch ); break;
		case EType::MakeMorph:		return NodeMeshMakeMorph::StaticUnserialise( arch ); break;
		case EType::Switch:			return NodeMeshSwitch::StaticUnserialise( arch ); break;
        case EType::Fragment:		return NodeMeshFragment::StaticUnserialise( arch ); break;
		case EType::Transform:		return NodeMeshTransform::StaticUnserialise( arch ); break;
		case EType::ClipMorphPlane: return NodeMeshClipMorphPlane::StaticUnserialise( arch ); break;
        case EType::ClipWithMesh:	return NodeMeshClipWithMesh::StaticUnserialise( arch ); break;
        case EType::ApplyPose:		return NodeMeshApplyPose::StaticUnserialise( arch ); break;
		case EType::Variation:		return NodeMeshVariation::StaticUnserialise( arch ); break;
		case EType::GeometryOperation:	return NodeMeshGeometryOperation::StaticUnserialise( arch ); break;
		case EType::Reshape:			return NodeMeshReshape::StaticUnserialise( arch ); break;
		case EType::ClipDeform:			return NodeMeshClipDeform::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeMesh::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeMesh::GetStaticType()
	{
		return &s_nodeMeshType;
	}


}


