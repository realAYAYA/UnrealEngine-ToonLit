// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/NodeSurfaceNewPrivate.h"
#include "MuT/NodeModifier.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceVariation::Private : public NodeSurface::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:


		static NODE_TYPE s_type;

        TArray<NodeSurfacePtr> m_defaultSurfaces;
		TArray<NodeModifierPtr> m_defaultModifiers;

		struct VARIATION
		{
			TArray<NodeSurfacePtr> m_surfaces;
			TArray<NodeModifierPtr> m_modifiers;
            string m_tag;

			//!
			void Serialise( OutputArchive& arch ) const
			{
                uint32_t ver = 1;
				arch << ver;

				arch << m_tag;
                arch << m_surfaces;
                arch << m_modifiers;
            }

			void Unserialise( InputArchive& arch )
			{
                uint32_t ver = 0;
				arch >> ver;
                check(ver==1);

				arch >> m_tag;
				arch >> m_surfaces;
                arch >> m_modifiers;
			}
		};

        NodeSurfaceVariation::VariationType m_type = NodeSurfaceVariation::VariationType::Tag;

		TArray<VARIATION> m_variations;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 3;
			arch << ver;

            arch << uint32_t(m_type);
            arch << m_defaultSurfaces;
            arch << m_defaultModifiers;
            arch << m_variations;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check( ver==3 );

            uint32_t temp;
            arch >> temp;
            m_type = NodeSurfaceVariation::VariationType(temp);

            arch >> m_defaultSurfaces;
            arch >> m_defaultModifiers;
            arch >> m_variations;
		}
	};

}
