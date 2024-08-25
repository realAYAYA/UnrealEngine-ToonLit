// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeModifier.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeModifierMeshClipWithUVMask.h"
#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{
	// Static initialisation
	static FNodeType s_nodeModifierType = FNodeType( "NodeModifier", Node::GetStaticType() );

	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EMutableMultipleTagPolicy);

	const FNodeType* NodeModifier::GetType() const
	{
		return GetStaticType();
	}


	const FNodeType* NodeModifier::GetStaticType()
	{
		return &s_nodeModifierType;
    }


	void NodeModifier::Serialise( const NodeModifier* p, OutputArchive& arch )
	{
        uint32 ver = 0;
		arch << ver;

	#define SERIALISE_CHILDREN( C, ID ) \
		( p->GetType()==C::GetStaticType() )					\
		{ 														\
			const C* pTyped = static_cast<const C*>(p);			\
            arch << (uint32)ID;									\
			C::Serialise( pTyped, arch );						\
		}														\

		if SERIALISE_CHILDREN(NodeModifierMeshClipMorphPlane, 			0 )
		else if SERIALISE_CHILDREN(NodeModifierMeshClipWithMesh, 		1 )
		else if SERIALISE_CHILDREN(NodeModifierMeshClipDeform, 		    2 )
		else if SERIALISE_CHILDREN(NodeModifierMeshClipWithUVMask, 		3 )
		else check(false);

#undef SERIALISE_CHILDREN
    }

        
	NodeModifierPtr NodeModifier::StaticUnserialise( InputArchive& arch )
	{
        uint32 ver;
		arch >> ver;
		check( ver == 0 );

        uint32 id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeModifierMeshClipMorphPlane::StaticUnserialise( arch ); break;
		case 1 :  return NodeModifierMeshClipWithMesh::StaticUnserialise( arch ); break;
		case 2 :  return NodeModifierMeshClipDeform::StaticUnserialise(arch); break;
		case 3 :  return NodeModifierMeshClipWithUVMask::StaticUnserialise(arch); break;
		default : check(false);
		}

		return 0;
	}

	void NodeModifier::AddTag(const FString& Value)
	{
		NodeModifier::Private* pD = static_cast<NodeModifier::Private*>(GetBasePrivate());
		pD->RequiredTags.Add(Value);
	}


	void NodeModifier::SetMultipleTagPolicy(EMutableMultipleTagPolicy Value)
	{
		NodeModifier::Private* pD = static_cast<NodeModifier::Private*>(GetBasePrivate());
		pD->MultipleTagsPolicy = Value;
	}


	void NodeModifier::SetStage(bool bBeforeNormalOperation)
	{
		NodeModifier::Private* pD = static_cast<NodeModifier::Private*>(GetBasePrivate());
		pD->bApplyBeforeNormalOperations = bBeforeNormalOperation; 
	}

}


