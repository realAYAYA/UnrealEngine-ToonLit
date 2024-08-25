// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodePatchImage.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImage.h"
#include "MuT/NodePatchImagePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{


    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    FNodeType NodePatchImage::Private::s_type = FNodeType( "PatchTexture", Node::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodePatchImage, EType::PatchImage, Node, Node::EType::None)


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetImage( NodeImagePtr pImage )
    {
        m_pD->m_pImage = pImage;
    }


    //---------------------------------------------------------------------------------------------
    NodeImagePtr NodePatchImage::GetImage() const
    {
        return m_pD->m_pImage.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetMask( NodeImagePtr pMask )
    {
        m_pD->m_pMask = pMask;
    }


    //---------------------------------------------------------------------------------------------
    NodeImagePtr NodePatchImage::GetMask() const
    {
        return m_pD->m_pMask.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetBlockCount( int c )
    {
        m_pD->m_blocks.SetNum( c );
    }


    //---------------------------------------------------------------------------------------------
    int NodePatchImage::GetBlockCount() const
    {
        return (int)m_pD->m_blocks.Num();
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetBlock( int index, int block )
    {
        check( index>=0 && index<(int)m_pD->m_blocks.Num() );
        m_pD->m_blocks[ index ] = block;
    }


    //---------------------------------------------------------------------------------------------
    int NodePatchImage::GetBlock( int index ) const
    {
        check( index>=0 && index<(int)m_pD->m_blocks.Num() );
        return m_pD->m_blocks[ index ];
    }


    //---------------------------------------------------------------------------------------------
	EBlendType NodePatchImage::GetBlendType() const
    {
        return m_pD->m_blendType;
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetBlendType(EBlendType t)
    {
        m_pD->m_blendType = t;
    }


    //---------------------------------------------------------------------------------------------
    bool NodePatchImage::GetApplyToAlphaChannel() const
    {
        return m_pD->m_applyToAlpha;
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetApplyToAlphaChannel(bool a )
    {
        m_pD->m_applyToAlpha = a;
    }

}
