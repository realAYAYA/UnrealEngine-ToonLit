// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshInterpolate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/AST.h"

#include "MuR/MeshPrivate.h"


namespace mu
{


	class NodeMeshInterpolate::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeScalarPtr m_pFactor;
		TArray<NodeMeshPtr> m_targets;

		//!
		struct CHANNEL
		{
			CHANNEL()
			{
				semantic = MBS_NONE;
				semanticIndex = 0;
			}

			CHANNEL( MESH_BUFFER_SEMANTIC asemantic,
                     int32_t asemanticIndex )
			{
				semantic = asemantic;
				semanticIndex = asemanticIndex;
			}

			MESH_BUFFER_SEMANTIC semantic;
            int32_t semanticIndex;

			//-------------------------------------------------------------------------------------
			void Serialise( OutputArchive& arch ) const
			{
                const int32_t ver = 0;
				arch << ver;

				arch << semantic;
				arch << semanticIndex;
			}

			//-------------------------------------------------------------------------------------
			void Unserialise( InputArchive& arch )
			{
                int32_t ver = 0;
				arch >> ver;
				check(ver == 0);

				arch >> semantic;
				arch >> semanticIndex;
			}
		};

		TArray<CHANNEL> m_channels;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_pFactor;
			arch << m_targets;
			arch << m_channels;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==1);

			arch >> m_pFactor;
			arch >> m_targets;

			if (ver>=1)
			{
				arch >> m_channels;
			}
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};


}
