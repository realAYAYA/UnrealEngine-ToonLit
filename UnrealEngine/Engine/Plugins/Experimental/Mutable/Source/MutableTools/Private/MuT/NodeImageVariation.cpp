// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageVariationPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    FNodeType NodeImageVariation::Private::s_type =
        FNodeType( "ImageVariation", NodeImage::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageVariation, EType::Variation, Node, Node::EType::Image)


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetDefaultImage( NodeImage* p ) 
	{ 
		m_pD->m_defaultImage = p; 
	}


	int NodeImageVariation::GetVariationCount() const 
	{ 
		return int( m_pD->m_variations.Num() ); 
	}


    void NodeImageVariation::SetVariationCount( int num )
    {
        check( num >= 0 );
        m_pD->m_variations.SetNum( num );
    }


	void NodeImageVariation::SetVariationTag( int index, const FString& Tag )
    {
        check( index >= 0 && index < (int)m_pD->m_variations.Num() );

		m_pD->m_variations[index].m_tag = Tag;
    }


    void NodeImageVariation::SetVariationImage( int index, NodeImage* pNode )
    {
        check( index >= 0 && index < (int)m_pD->m_variations.Num() );

        m_pD->m_variations[index].m_image = pNode;
    }


} // namespace mu
