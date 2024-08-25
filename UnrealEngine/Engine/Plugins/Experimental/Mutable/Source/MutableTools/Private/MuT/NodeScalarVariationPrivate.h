// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeScalarVariation.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    class NodeScalarVariation::Private : public Node::Private
    {
    public:

        static FNodeType s_type;

        NodeScalarPtr m_defaultScalar;

        struct FVariation
        {
            NodeScalarPtr m_scalar;
            FString m_tag;

            //!
            void Serialise( OutputArchive& arch ) const
            {
                uint32 ver = 2;
                arch << ver;

                arch << m_tag;
                arch << m_scalar;
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
				
				arch >> m_scalar;
            }
        };

        TArray<FVariation> m_variations;

        //!
        void Serialise( OutputArchive& arch ) const
        {
            uint32 ver = 1;
            arch << ver;

            arch << m_defaultScalar;
            arch << m_variations;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32 ver;
            arch >> ver;
			check(ver == 1);

            arch >> m_defaultScalar;
            arch >> m_variations;
        }
    };

} // namespace mu
