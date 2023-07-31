// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodePatchImagePrivate.h"

#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceEdit::Private : public Node::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

        NodeSurfacePtr m_pParent;
		NodePatchMeshPtr m_pMesh;
		NodeMeshPtr m_pMorph;

        //! This flag indicates that the mesh in the m_pMorph connection is a target mesh, and not
        //! morph information. This means that the morph information needs to be generated.
        bool m_morphIsTarget = true;

		struct TEXTURE
		{
			NodeImagePtr m_pExtend;
			NodePatchImagePtr m_pPatch;

			//!
			void Serialise( OutputArchive& arch ) const
			{
				arch << m_pExtend;
				arch << m_pPatch;
			}

			void Unserialise( InputArchive& arch )
			{
				arch >> m_pExtend;
				arch >> m_pPatch;
			}
		};

		TArray<TEXTURE> m_textures;

        //! Tags in this surface edit
		TArray<mu::string> m_tags;

		//! Factor of the morph
		NodeScalarPtr m_pFactor;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 4;
			arch << ver;

			arch << m_pParent;
			arch << m_pMesh;
            arch << m_pMorph;
            arch << m_morphIsTarget;
            arch << m_textures;
            arch << m_tags;
			arch << m_pFactor;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check( ver==4 );

			arch >> m_pParent;
			arch >> m_pMesh;
			arch >> m_pMorph;
            arch >> m_morphIsTarget;
			arch >> m_textures;
            arch >> m_tags;
			arch >> m_pFactor;
        }
	};

}

