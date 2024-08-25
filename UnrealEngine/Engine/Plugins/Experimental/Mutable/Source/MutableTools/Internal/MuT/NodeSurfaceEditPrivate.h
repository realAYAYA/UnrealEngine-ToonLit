// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeSurfaceEdit.h"

#include "MuT/NodePrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodePatchImage.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceEdit::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

        NodeSurfacePtr m_pParent;
		NodePatchMeshPtr m_pMesh;
		NodeMeshPtr m_pMorph;

        //! This flag indicates that the mesh in the m_pMorph connection is a target mesh, and not
        //! morph information. This means that the morph information needs to be generated.
        bool m_morphIsTarget = true;

		struct FTexture
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

		TArray<FTexture> m_textures;

        //! Tags in this surface edit
		TArray<FString> m_tags;

		//! Factor of the morph
		NodeScalarPtr m_pFactor;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 5;
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
            check( ver>=4 && ver<=5 );

			arch >> m_pParent;
			arch >> m_pMesh;
			arch >> m_pMorph;
            arch >> m_morphIsTarget;
			arch >> m_textures;
			if (ver <= 4)
			{
				TArray <std::string> Temp;
				arch >> Temp;
				m_tags.SetNum(Temp.Num());
				for (int32 i=0; i<Temp.Num(); ++i)
				{
					m_tags[i] = Temp[i].c_str();
				}
			}
			else
			{
				arch >> m_tags;
			}
			arch >> m_pFactor;
        }
	};

}

