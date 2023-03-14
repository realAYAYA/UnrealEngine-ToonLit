// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarVariation.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    class NodeScalarVariation::Private : public Node::Private
    {
    public:
        MUTABLE_DEFINE_CONST_VISITABLE()

    public:

        static NODE_TYPE s_type;

        NodeScalarPtr m_defaultScalar;

        struct VARIATION
        {
            NodeScalarPtr m_scalar;
            string m_tag;

            //!
            void Serialise( OutputArchive& arch ) const
            {
                uint32_t ver = 1;
                arch << ver;

                arch << m_tag;
                arch << m_scalar;
            }

            void Unserialise( InputArchive& arch )
            {
                uint32_t ver = 0;
                arch >> ver;
                check( ver == 1 );

                arch >> m_tag;
                arch >> m_scalar;
            }
        };

        TArray<VARIATION> m_variations;

        //!
        void Serialise( OutputArchive& arch ) const
        {
            uint32_t ver = 1;
            arch << ver;

            arch << m_defaultScalar;
            arch << m_variations;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32_t ver;
            arch >> ver;
			check(ver == 1);

            arch >> m_defaultScalar;
            arch >> m_variations;
        }
    };

} // namespace mu
