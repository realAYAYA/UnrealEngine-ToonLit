// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeColourVariationPrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    NODE_TYPE NodeColourVariation::Private::s_type =
        NODE_TYPE( "ColourVariation", NodeColour::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeColourVariation, EType::Variation, Node, Node::EType::Colour)


    //---------------------------------------------------------------------------------------------
    int NodeColourVariation::GetInputCount() const { return 1 + m_pD->m_variations.Num(); }


    //---------------------------------------------------------------------------------------------
    Node* NodeColourVariation::GetInputNode( int i ) const
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            return m_pD->m_defaultColour.get();
        }
        i -= 1;

        if ( i < int( m_pD->m_variations.Num() ) )
        {
            return m_pD->m_variations[i].m_colour.get();
        }
        i -= int( m_pD->m_variations.Num() );

        return nullptr;
    }


    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetInputNode( int i, NodePtr pNode )
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            m_pD->m_defaultColour = dynamic_cast<NodeColour*>( pNode.get() );
            return;
        }

        i -= 1;
        if ( i < int( m_pD->m_variations.Num() ) )
        {

            m_pD->m_variations[i].m_colour = dynamic_cast<NodeColour*>( pNode.get() );
            return;
        }
        i -= (int)m_pD->m_variations.Num();
    }


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetDefaultColour( NodeColour* p ) { m_pD->m_defaultColour = p; }


    //---------------------------------------------------------------------------------------------
    int NodeColourVariation::GetVariationCount() const { return m_pD->m_variations.Num(); }


    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetVariationCount( int num )
    {
        check( num >= 0 );
        m_pD->m_variations.SetNum( num );
    }

    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetVariationTag( int index, const char* strTag )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );
        check( strTag );

        if ( strTag )
        {
            m_pD->m_variations[index].m_tag = strTag;
        }
        else
        {
            m_pD->m_variations[index].m_tag = "";
        }
    }


    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetVariationColour( int index, NodeColour* pNode )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );

        m_pD->m_variations[index].m_colour = pNode;
    }


} // namespace mu
