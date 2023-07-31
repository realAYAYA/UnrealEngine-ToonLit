// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeComponentEdit.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeComponentEditPrivate.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurface.h"

#include <memory>
#include <utility>


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeComponentEdit::Private::s_type =
			NODE_TYPE( "EditComponent", NodeComponent::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeComponentEdit, EType::Edit, Node, Node::EType::Component)


    //---------------------------------------------------------------------------------------------
    // Node Interface
    //---------------------------------------------------------------------------------------------
    int NodeComponentEdit::GetInputCount() const
    {
        return m_pD->m_surfaces.Num();
    }


    //---------------------------------------------------------------------------------------------
    Node* NodeComponentEdit::GetInputNode( int i ) const
    {
        check( i >=0 && i < GetInputCount() );

        NodePtr pResult;

        if ( i<m_pD->m_surfaces.Num() )
        {
            pResult = m_pD->m_surfaces[i];
        }

        return pResult.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeComponentEdit::SetInputNode( int i, NodePtr pNode )
    {
        check( i >=0 && i < GetInputCount() );

        if ( i<m_pD->m_surfaces.Num() )
        {
            m_pD->m_surfaces[ i ] = dynamic_cast<NodeSurface*>(pNode.get());
        }
    }


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

}


