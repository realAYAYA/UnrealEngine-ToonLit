// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshClipWithMesh.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"
#include "MuR/MutableMath.h"


namespace mu
{


    class NodeMeshClipWithMesh::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeMeshPtr m_pSource;
		NodeMeshPtr m_pClipMesh;

		TArray<mu::string> m_tags;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 1;
			arch << ver;

			arch << m_pSource;
			arch << m_pClipMesh;
			arch << m_tags;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;

			arch >> m_pSource;
			arch >> m_pClipMesh;
			arch >> m_tags;
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};


}
