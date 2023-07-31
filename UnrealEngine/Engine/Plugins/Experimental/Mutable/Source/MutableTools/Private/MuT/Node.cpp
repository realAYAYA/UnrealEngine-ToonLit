// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Node.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeModifier.h"
#include "MuT/NodeObject.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeString.h"
#include "MuT/NodeSurface.h"
#include "map"

#include <stdint.h>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeType = 		NODE_TYPE( "Node", 0 );


	//---------------------------------------------------------------------------------------------
	NODE_TYPE::NODE_TYPE()
	{
		m_strName = "";
		m_pParent = 0;
	}

	//---------------------------------------------------------------------------------------------
	NODE_TYPE::NODE_TYPE( const char* strName, const NODE_TYPE* pParent )
	{
        m_strName = strName;
		m_pParent = pParent;
	}



	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	void Node::Serialise( const Node* pNode, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		// Warning: cannot be replaced with the wrapping virtual call because some subclasses are abstract
		arch << uint32_t(pNode->Type);

		#define SERIALISE_CHILDREN( C, ID ) C::Serialise(static_cast<const C*>(pNode), arch );

		switch (pNode->Type)
		{
		case EType::Colour:		SERIALISE_CHILDREN(NodeColour,arch); break;
		case EType::Component:  SERIALISE_CHILDREN(NodeComponent,arch); break;
		case EType::Image:		SERIALISE_CHILDREN(NodeImage,arch); break;
		case EType::Layout:		SERIALISE_CHILDREN(NodeLayout,arch); break;
		case EType::LOD:		SERIALISE_CHILDREN(NodeLOD,arch); break;
		case EType::Mesh:		SERIALISE_CHILDREN(NodeMesh,arch); break;
		case EType::Object:		SERIALISE_CHILDREN(NodeObject,arch); break;
		case EType::PatchImage: SERIALISE_CHILDREN(NodePatchImage,arch); break;
		case EType::Scalar:		SERIALISE_CHILDREN(NodeScalar,arch); break;
		case EType::PatchMesh:	SERIALISE_CHILDREN(NodePatchMesh, arch); break;
		case EType::Projector:	SERIALISE_CHILDREN(NodeProjector,arch); break;
		case EType::Surface:	SERIALISE_CHILDREN(NodeSurface,arch); break;
		case EType::Modifier:	SERIALISE_CHILDREN(NodeModifier,arch); break;
		case EType::Range:		SERIALISE_CHILDREN(NodeRange,arch); break;
		case EType::String:		SERIALISE_CHILDREN(NodeString, arch); break;
		case EType::Bool:		SERIALISE_CHILDREN(NodeBool, arch); break;
		default: check(false);
		}

		#undef SERIALISE_CHILDREN
	}


	//---------------------------------------------------------------------------------------------
	NodePtr Node::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (EType(id))
		{
		case EType::Colour:		return NodeColour::StaticUnserialise( arch ); break;
		case EType::Component:  return NodeComponent::StaticUnserialise( arch ); break;
		case EType::Image:		return NodeImage::StaticUnserialise( arch ); break;
		case EType::Layout:		return NodeLayout::StaticUnserialise( arch ); break;
		case EType::LOD:		return NodeLOD::StaticUnserialise( arch ); break;
		case EType::Mesh:		return NodeMesh::StaticUnserialise( arch ); break;
		case EType::Object:		return NodeObject::StaticUnserialise( arch ); break;
		case EType::PatchImage: return NodePatchImage::StaticUnserialise( arch ); break;
		case EType::Scalar:		return NodeScalar::StaticUnserialise( arch ); break;
		case EType::PatchMesh:	return NodePatchMesh::StaticUnserialise( arch ); break;
		case EType::Projector:	return NodeProjector::StaticUnserialise( arch ); break;
		case EType::Surface:	return NodeSurface::StaticUnserialise( arch ); break;
		case EType::Modifier:	return NodeModifier::StaticUnserialise( arch ); break;
		case EType::Range:		return NodeRange::StaticUnserialise( arch ); break;
		case EType::String:		return NodeString::StaticUnserialise( arch ); break;
		case EType::Bool:		return NodeBool::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}



	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* Node::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* Node::GetStaticType()
	{
		return &s_nodeType;
	}


	//---------------------------------------------------------------------------------------------
	void Node::SetMessageContext( const void* context )
	{
		GetBasePrivate()->m_errorContext = context;
	}


}


