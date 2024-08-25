// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColourVariationPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    FNodeType NodeColourVariation::Private::s_type =
        FNodeType( "ColourVariation", NodeColour::GetStaticType() );


    MUTABLE_IMPLEMENT_NODE( NodeColourVariation, EType::Variation, Node, Node::EType::Colour)


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetDefaultColour( NodeColour* p ) { m_pD->m_defaultColour = p; }


    //---------------------------------------------------------------------------------------------
    int NodeColourVariation::GetVariationCount() const { return m_pD->m_variations.Num(); }


    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetVariationCount( int32 num )
    {
        check( num >= 0 );
        m_pD->m_variations.SetNum( num );
    }

    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetVariationTag( int32 index, const FString& Tag )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );
        m_pD->m_variations[index].m_tag = Tag;
    }


    //---------------------------------------------------------------------------------------------
    void NodeColourVariation::SetVariationColour( int32 index, NodeColour* pNode )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );

        m_pD->m_variations[index].m_colour = pNode;
    }


} // namespace mu
