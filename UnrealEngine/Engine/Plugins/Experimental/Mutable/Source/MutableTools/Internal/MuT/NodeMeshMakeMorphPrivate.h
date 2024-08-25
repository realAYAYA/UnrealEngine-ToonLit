// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshMakeMorph.h"
#include "MuT/AST.h"


namespace mu
{


    class NodeMeshMakeMorph::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		NodeMeshPtr m_pBase;
		NodeMeshPtr m_pTarget;
		
		bool bOnlyPositionAndNormal = false;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_pBase;
			arch << m_pTarget;
			arch << bOnlyPositionAndNormal;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver <= 1);

			arch >> m_pBase;
			arch >> m_pTarget;

			if (ver == 1)
			{
				arch >> bOnlyPositionAndNormal;
			}
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};


}
