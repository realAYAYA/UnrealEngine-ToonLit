// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeImageVariationPrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    NODE_TYPE NodeImageVariation::Private::s_type =
        NODE_TYPE( "ImageVariation", NodeImage::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageVariation, EType::Variation, Node, Node::EType::Image)


    //---------------------------------------------------------------------------------------------
    int NodeImageVariation::GetInputCount() const { return 1 + int( m_pD->m_variations.Num() ); }


    //---------------------------------------------------------------------------------------------
    Node* NodeImageVariation::GetInputNode( int i ) const
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            return m_pD->m_defaultImage.get();
        }
        i -= 1;

        if ( i < int( m_pD->m_variations.Num() ) )
        {
            return m_pD->m_variations[i].m_image.get();
        }
        i -= int( m_pD->m_variations.Num() );

        return nullptr;
    }


    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetInputNode( int i, NodePtr pNode )
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            m_pD->m_defaultImage = dynamic_cast<NodeImage*>( pNode.get() );
            return;
        }

        i -= 1;
        if ( i < int( m_pD->m_variations.Num() ) )
        {

            m_pD->m_variations[i].m_image = dynamic_cast<NodeImage*>( pNode.get() );
            return;
        }
        i -= (int)m_pD->m_variations.Num();
    }


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetDefaultImage( NodeImage* p ) { m_pD->m_defaultImage = p; }


    //---------------------------------------------------------------------------------------------
    int NodeImageVariation::GetVariationCount() const { return int( m_pD->m_variations.Num() ); }


    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetVariationCount( int num )
    {
        check( num >= 0 );
        m_pD->m_variations.SetNum( num );
    }

    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetVariationTag( int index, const char* strTag )
    {
        check( index >= 0 && index < (int)m_pD->m_variations.Num() );
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
    void NodeImageVariation::SetVariationImage( int index, NodeImage* pNode )
    {
        check( index >= 0 && index < (int)m_pD->m_variations.Num() );

        m_pD->m_variations[index].m_image = pNode;
    }


} // namespace mu
