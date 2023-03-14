// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodePatchImage.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MemoryPrivate.h"
#include "MuT/NodeImage.h"
#include "MuT/NodePatchImagePrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{


    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    NODE_TYPE NodePatchImage::Private::s_type =
            NODE_TYPE( "PatchTexture", NodePatchImage::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodePatchImage, EType::PatchImage, Node, Node::EType::PatchImage);


    //---------------------------------------------------------------------------------------------
    // Node Interface
    //---------------------------------------------------------------------------------------------
    int NodePatchImage::GetInputCount() const
    {
        return 2;
    }


    //---------------------------------------------------------------------------------------------
    Node* NodePatchImage::GetInputNode( int i ) const
    {
        check( i >=0 && i < GetInputCount());

        Node* pResult = 0;

        if ( i==0 )
        {
            pResult = m_pD->m_pImage.get();
        }
        else
        {
            pResult = m_pD->m_pMask.get();
        }

        return pResult;
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetInputNode( int i, NodePtr pNode )
    {
        check( i >=0 && i < GetInputCount());

        if ( i==0 )
        {
            m_pD->m_pImage = dynamic_cast<NodeImage*>( pNode.get() );
        }
        else
        {
            m_pD->m_pMask = dynamic_cast<NodeImage*>( pNode.get() );
        }
    }


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
