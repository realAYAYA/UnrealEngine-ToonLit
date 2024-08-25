// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarVariationPrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    FNodeType NodeScalarVariation::Private::s_type =
        FNodeType( "ScalarVariation", NodeScalar::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeScalarVariation, EType::Variation, Node, Node::EType::Scalar)


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodeScalarVariation::SetDefaultScalar( NodeScalar* p ) 
	{ 
		m_pD->m_defaultScalar = p; 
	}


	void NodeScalarVariation::SetVariationCount( int32 num )
    {
        check( num >= 0 );
        m_pD->m_variations.SetNum( num );
    }


	void NodeScalarVariation::SetVariationTag( int32 index, const FString& Tag )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );

		m_pD->m_variations[index].m_tag = Tag;
    }


    void NodeScalarVariation::SetVariationScalar( int32 index, NodeScalar* pNode )
    {
        check( index >= 0 && index < m_pD->m_variations.Num() );

        m_pD->m_variations[index].m_scalar = pNode;
    }


} // namespace mu
