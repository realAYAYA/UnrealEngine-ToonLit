// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeComponentEdit.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeComponentEditPrivate.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurface.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeComponentEdit::Private::s_type =
			FNodeType( "EditComponent", NodeComponent::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeComponentEdit, EType::Edit, Node, Node::EType::Component)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeComponentEdit::SetParent( NodeComponent* p )
	{
		m_pD->m_pParent = p;
	}


	//---------------------------------------------------------------------------------------------
    NodeComponent* NodeComponentEdit::GetParent() const
	{
        return m_pD->m_pParent.get();
	}


    //---------------------------------------------------------------------------------------------
    int NodeComponentEdit::GetSurfaceCount() const
    {
        return m_pD->m_surfaces.Num();
    }


    //---------------------------------------------------------------------------------------------
    void NodeComponentEdit::SetSurfaceCount( int num )
    {
        check( num >=0 );
        m_pD->m_surfaces.SetNum( num );
    }


    //---------------------------------------------------------------------------------------------
    NodeSurface* NodeComponentEdit::GetSurface( int index ) const
    {
        check( index >=0 && index < m_pD->m_surfaces.Num() );

        return m_pD->m_surfaces[ index ].get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeComponentEdit::SetSurface( int index, NodeSurface* pNode )
    {
        check( index >=0 && index < m_pD->m_surfaces.Num() );

        m_pD->m_surfaces[ index ] = pNode;
    }


	//---------------------------------------------------------------------------------------------
	const NodeComponentNew::Private* NodeComponentEdit::Private::GetParentComponentNew() const
	{
		const NodeComponentNew::Private* parent = nullptr;
		if (m_pParent)
		{
			NodeComponent::Private* ParentPrivate = static_cast<NodeComponent::Private*>(m_pParent->GetBasePrivate());
			parent = ParentPrivate->GetParentComponentNew();
		}

		check(parent);
		return parent;
	}

}


