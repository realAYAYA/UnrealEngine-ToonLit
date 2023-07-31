// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeColourVariation.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    class NodeColourVariation::Private : public Node::Private
    {
    public:
        MUTABLE_DEFINE_CONST_VISITABLE()

    public:
        Private() {}

        static NODE_TYPE s_type;

        NodeColourPtr m_defaultColour;

        struct VARIATION
        {
            NodeColourPtr m_colour;
            string m_tag;

            //!
            void Serialise( OutputArchive& arch ) const
            {
                uint32_t ver = 1;
                arch << ver;

                arch << m_tag;
                arch << m_colour;
            }

            void Unserialise( InputArchive& arch )
            {
                uint32_t ver = 0;
                arch >> ver;
                check( ver == 1 );

                arch >> m_tag;
                arch >> m_colour;
            }
        };

        TArray<VARIATION> m_variations;

        //!
        void Serialise( OutputArchive& arch ) const
        {
            uint32_t ver = 1;
            arch << ver;

            arch << m_defaultColour;
            arch << m_variations;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32_t ver;
            arch >> ver;
			check(ver == 1);

            arch >> m_defaultColour;
            arch >> m_variations;
        }
    };

} // namespace mu

