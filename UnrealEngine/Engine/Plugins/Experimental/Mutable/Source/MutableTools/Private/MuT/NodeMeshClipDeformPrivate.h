// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshClipDeform.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/AST.h"

namespace mu
{

	class NodeMeshClipDeform::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		Ptr<NodeMesh> m_pBaseMesh;
		Ptr<NodeMesh> m_pClipShape;
		Ptr<NodeImage> m_pShapeWeights;

        //!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pBaseMesh;
			arch << m_pClipShape;
			arch << m_pShapeWeights;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver == 0);

			arch >> m_pBaseMesh;
			arch >> m_pClipShape;
			arch >> m_pShapeWeights;	
		}

		// NodeMesh::Private interface
        Ptr<NodeLayout> GetLayout( int index ) const override;

	};

}
