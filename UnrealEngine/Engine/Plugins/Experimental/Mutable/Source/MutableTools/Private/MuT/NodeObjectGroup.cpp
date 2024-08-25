// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeObjectGroup.h"

#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeObjectGroupPrivate.h"
#include "MuT/NodeObjectPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(NodeObjectGroup::CHILD_SELECTION)


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeObjectGroup::Private::s_type = FNodeType( "ObjectGroup", NodeObject::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeObjectGroup, EType::Group, Node, Node::EType::Object)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	const FString& NodeObjectGroup::GetName() const
	{
		return m_pD->Name;
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectGroup::SetName( const FString& Name )
	{
		m_pD->Name = Name;
	}


	const FString& NodeObjectGroup::GetUid() const
	{
		return m_pD->Uid;
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectGroup::SetUid( const FString& Uid )
	{
		m_pD->Uid = Uid;
	}


	//---------------------------------------------------------------------------------------------
	NodeObjectGroup::CHILD_SELECTION NodeObjectGroup::GetSelectionType() const
	{
		return m_pD->m_type;
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectGroup::SetSelectionType( CHILD_SELECTION t )
	{
		m_pD->m_type = t;
	}


	//---------------------------------------------------------------------------------------------
	int NodeObjectGroup::GetChildCount() const
	{
		return m_pD->m_children.Num();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectGroup::SetChildCount( int num )
	{
		check( num >=0 );
		m_pD->m_children.SetNum( num );
	}


	//---------------------------------------------------------------------------------------------
	NodeObjectPtr NodeObjectGroup::GetChild( int index ) const
	{
		check( index >=0 && index < m_pD->m_children.Num() );

		return m_pD->m_children[ index ].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeObjectGroup::SetChild( int index, NodeObjectPtr pObject )
	{
		check( index >=0 && index < m_pD->m_children.Num() );

		m_pD->m_children[ index ] = pObject;
	}


	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeObjectGroup::Private::GetLayout(int lod, int component, int surface, int texture ) const
	{
		NodeLayoutPtr pLayout;

		for ( int32 i=0; !pLayout && i<m_children.Num(); ++i )
		{
			if (m_children[i])
			{
				NodeObject::Private* pPrivate = static_cast<NodeObject::Private*>( m_children[i]->GetBasePrivate() );
                pLayout = pPrivate->GetLayout( lod, component, surface, texture );
			}
		}

		// TODO: layout index
		return pLayout;
	}

	void NodeObjectGroup::SetDefaultValue(int32 Value)
	{
		m_pD->DefaultValue = Value;
	}

}


