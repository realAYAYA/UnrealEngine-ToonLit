// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeImageVariation.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    class NodeImageVariation::Private : public Node::Private
    {
    public:
        MUTABLE_DEFINE_CONST_VISITABLE()

    public:
        Private() {}

        static NODE_TYPE s_type;

        NodeImagePtr m_defaultImage;

        struct VARIATION
        {
            NodeImagePtr m_image;
            string m_tag;

            //!
            void Serialise( OutputArchive& arch ) const
            {
                uint32_t ver = 1;
                arch << ver;

                arch << m_tag;
                arch << m_image;
            }

            void Unserialise( InputArchive& arch )
            {
                uint32_t ver = 0;
                arch >> ver;
                check( ver == 1 );

                arch >> m_tag;
                arch >> m_image;
            }
        };

		TArray<VARIATION> m_variations;

        //!
        void Serialise( OutputArchive& arch ) const
        {
            uint32_t ver = 1;
            arch << ver;

            arch << m_defaultImage;
            arch << m_variations;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32_t ver;
            arch >> ver;
			check(ver == 1);

            arch >> m_defaultImage;
            arch >> m_variations;
        }
    };

} // namespace mu

