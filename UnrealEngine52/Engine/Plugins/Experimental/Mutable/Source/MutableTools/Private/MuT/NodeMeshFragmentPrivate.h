// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeMeshFragment::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		Private()
		{
            m_layoutOrGroup = -1;
		}

		static NODE_TYPE s_type;

		NodeMeshPtr m_pMesh;
        int m_layoutOrGroup;
		TArray<int> m_blocks;

        FRAGMENT_TYPE m_fragmentType;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_pMesh;
            arch << (int32_t)m_fragmentType;
            arch << m_layoutOrGroup;
			arch << m_blocks;

		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==1);

			arch >> m_pMesh;

            int32_t temp;
            arch >> temp;
            m_fragmentType = (FRAGMENT_TYPE)temp;

            arch >> m_layoutOrGroup;
			arch >> m_blocks;
		}


		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;
	};


}
