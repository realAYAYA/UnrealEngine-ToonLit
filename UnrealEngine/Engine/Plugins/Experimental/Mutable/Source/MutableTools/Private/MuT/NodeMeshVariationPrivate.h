// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeMeshVariation.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    class NodeMeshVariation::Private : public Node::Private
    {
    public:
        MUTABLE_DEFINE_CONST_VISITABLE()

    public:
        Private() {}

        static NODE_TYPE s_type;

        NodeMeshPtr m_defaultMesh;

        struct VARIATION
        {
            NodeMeshPtr m_mesh;
            string m_tag;

            //!
            void Serialise( OutputArchive& arch ) const
            {
                uint32_t ver = 1;
                arch << ver;

                arch << m_tag;
                arch << m_mesh;
            }

            void Unserialise( InputArchive& arch )
            {
                uint32_t ver = 0;
                arch >> ver;
                check( ver == 1 );

                arch >> m_tag;
                arch >> m_mesh;
            }
        };

        TArray<VARIATION> m_variations;

        //!
        void Serialise( OutputArchive& arch ) const
        {
            uint32_t ver = 1;
            arch << ver;

            arch << m_defaultMesh;
            arch << m_variations;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32_t ver;
            arch >> ver;
			check(ver == 1 );

            arch >> m_defaultMesh;
            arch >> m_variations;
        }
    };

} // namespace mu
