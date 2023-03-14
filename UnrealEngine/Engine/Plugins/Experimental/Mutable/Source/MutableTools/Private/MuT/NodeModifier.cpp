// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeModifier.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodePrivate.h"

#include <stdint.h>
#include <memory>
#include <utility>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeModifierType =
			NODE_TYPE( "NodeModifier", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeModifier::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeModifier::GetStaticType()
	{
          return &s_nodeModifierType;
        }


	//---------------------------------------------------------------------------------------------
	void NodeModifier::Serialise( const NodeModifier* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

#define SERIALISE_CHILDREN( C, ID ) \
        ( const C* pTyped##ID = dynamic_cast<const C*>(p) )			\
        {                                                           \
            arch << (uint32_t)ID;									\
            C::Serialise( pTyped##ID, arch );						\
		}

		if SERIALISE_CHILDREN(NodeModifierMeshClipMorphPlane, 			0 )
		else if SERIALISE_CHILDREN(NodeModifierMeshClipWithMesh, 		1 )
		else if SERIALISE_CHILDREN(NodeModifierMeshClipDeform, 		    2 )
		else check(false);

#undef SERIALISE_CHILDREN
    }

        
	//---------------------------------------------------------------------------------------------
	NodeModifierPtr NodeModifier::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeModifierMeshClipMorphPlane::StaticUnserialise( arch ); break;
		case 1 :  return NodeModifierMeshClipWithMesh::StaticUnserialise( arch ); break;
		case 2 :  return NodeModifierMeshClipDeform::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}

	//---------------------------------------------------------------------------------------------
	void NodeModifier::AddTag(const char* tagName)
	{
		NodeModifier::Private* pD = dynamic_cast<NodeModifier::Private*>(GetBasePrivate());
		check(pD);

		pD->m_tags.Add(tagName);
	}


    //---------------------------------------------------------------------------------------------
    int NodeModifier::GetTagCount() const
    {
        NodeModifier::Private* pD = dynamic_cast<NodeModifier::Private*>(GetBasePrivate());
        check(pD);

        return pD->m_tags.Num();
    }


    //---------------------------------------------------------------------------------------------
    const char* NodeModifier::GetTag( int i ) const
    {
        NodeModifier::Private* pD = dynamic_cast<NodeModifier::Private*>(GetBasePrivate());
        check(pD);

        if (i>=0 && i<GetTagCount())
        {
            return pD->m_tags[i].c_str();
        }
        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
	void NodeModifier::SetStage(bool bBeforeNormalOperation)
	{
		NodeModifier::Private* pD = dynamic_cast<NodeModifier::Private*>(GetBasePrivate());
		check(pD);

		pD->m_applyBeforeNormalOperations = bBeforeNormalOperation; 
	}

}


