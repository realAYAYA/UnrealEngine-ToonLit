// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/AST.h"


namespace mu
{


	class NodePatchMesh::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		NodeMeshPtr m_pRemove;
		NodeMeshPtr m_pAdd;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_pRemove;
			arch << m_pAdd;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check( ver==1 );

			arch >> m_pRemove;
			arch >> m_pAdd;
		}

	};


}
