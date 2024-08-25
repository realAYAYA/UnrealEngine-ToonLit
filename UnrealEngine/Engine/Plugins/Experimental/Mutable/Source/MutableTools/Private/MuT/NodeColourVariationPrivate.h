// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeColourVariation.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    class NodeColourVariation::Private : public Node::Private
    {
    public:
        Private() {}

        static FNodeType s_type;

        NodeColourPtr m_defaultColour;

        struct FVariation
        {
            NodeColourPtr m_colour;
			FString m_tag;

            //!
            void Serialise( OutputArchive& arch ) const
            {
                uint32 ver = 2;
                arch << ver;

                arch << m_tag;
                arch << m_colour;
            }

            void Unserialise( InputArchive& arch )
            {
                uint32 ver = 0;
                arch >> ver;
				check(ver >= 1 && ver <= 2);

				if (ver <= 1)
				{
					std::string Temp;
					arch >> Temp;
					m_tag = Temp.c_str();
				}
				else
				{
					arch >> m_tag;
				}
				arch >> m_colour;
            }
        };

        TArray<FVariation> m_variations;

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

