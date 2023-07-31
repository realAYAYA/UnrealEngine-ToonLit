// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{


	class NodeMeshFormat::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:


		static NODE_TYPE s_type;

		//! Source mesh to transform
		NodeMeshPtr m_pSource;

		//! New mesh format. The buffers in the sets have no elements, but they define the formats.
		//! If they are null it means that they are left with the original format.
		FMeshBufferSet m_VertexBuffers;
		FMeshBufferSet m_IndexBuffers;
		FMeshBufferSet m_FaceBuffers;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 4;
			arch << ver;

			arch << m_pSource;
			arch << m_VertexBuffers;
			arch << m_IndexBuffers;
			arch << m_FaceBuffers;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver>=3 && ver<=4);

			arch >> m_pSource;
			arch >> m_VertexBuffers;
			arch >> m_IndexBuffers;
			arch >> m_FaceBuffers;
			if (ver == 3)
			{
				bool bDummy;
				arch >> bDummy;
			}
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;
	};


}
