// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodeImage.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

    class NodePatchImage::Private : public Node::Private
    {
    public:

        MUTABLE_DEFINE_CONST_VISITABLE();

    public:

        static NODE_TYPE s_type;

        NodeImagePtr m_pImage;
        NodeImagePtr m_pMask;

        TArray<int> m_blocks;

        EBlendType m_blendType = EBlendType::BT_BLEND;

        // Patch alpha channel as well?
        bool m_applyToAlpha = false;

        //!
        void Serialise( OutputArchive& arch ) const
        {
            uint32_t ver = 2;
            arch << ver;

            arch << m_pImage;
            arch << m_pMask;
            arch << m_blocks;
            arch << (uint32_t)m_blendType;
            arch << m_applyToAlpha;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32_t ver;
            arch >> ver;
            check(ver==2);

            arch >> m_pImage;
            arch >> m_pMask;
            arch >> m_blocks;

			uint32_t t;
            arch >> t;
            m_blendType=(EBlendType)t;

            arch >> m_applyToAlpha;
        }
    };

}

