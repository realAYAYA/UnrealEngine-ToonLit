// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/NodeSurfaceNewPrivate.h"
#include "MuT/NodeModifier.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceVariation::Private : public NodeSurface::Private
	{
	public:

		static FNodeType s_type;

        TArray<NodeSurfacePtr> m_defaultSurfaces;
		TArray<NodeModifierPtr> m_defaultModifiers;

		struct FVariation
		{
			TArray<NodeSurfacePtr> m_surfaces;
			TArray<NodeModifierPtr> m_modifiers;
            FString m_tag;

			//!
			void Serialise( OutputArchive& arch ) const
			{
                uint32 ver = 2;
				arch << ver;

				arch << m_tag;
                arch << m_surfaces;
                arch << m_modifiers;
            }

			void Unserialise( InputArchive& arch )
			{
                uint32 ver = 0;
				arch >> ver;
                check(ver>=1&&ver<=2);

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
				arch >> m_surfaces;
                arch >> m_modifiers;
			}
		};

        NodeSurfaceVariation::VariationType m_type = NodeSurfaceVariation::VariationType::Tag;

		TArray<FVariation> m_variations;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 3;
			arch << ver;

            arch << uint32_t(m_type);
            arch << m_defaultSurfaces;
            arch << m_defaultModifiers;
            arch << m_variations;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check( ver==3 );

            uint32 temp;
            arch >> temp;
            m_type = NodeSurfaceVariation::VariationType(temp);

            arch >> m_defaultSurfaces;
            arch >> m_defaultModifiers;
            arch >> m_variations;
		}
	};

}
