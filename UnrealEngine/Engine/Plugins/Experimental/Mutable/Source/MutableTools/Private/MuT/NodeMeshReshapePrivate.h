// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshReshape.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeMeshPrivate.h"
#include "MuT/AST.h"

namespace mu
{


	class NodeMeshReshape::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		Ptr<NodeMesh> m_pBaseMesh;
		Ptr<NodeMesh> m_pBaseShape;
		Ptr<NodeMesh> m_pTargetShape;
		bool m_reshapeSkeleton = false;
		bool m_enableRigidParts = false;
		bool m_deformAllBones_DEPRECATED = false;
		bool m_deformAllPhysics = false;
		bool m_reshapePhysicsVolumes = false;
		
		TArray<string> m_bonesToDeform;
		TArray<string> m_physicsToDeform;
        //!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 6;
			arch << ver;

			arch << m_pBaseMesh;
			arch << m_pBaseShape;
			arch << m_pTargetShape;
			arch << m_reshapeSkeleton;
			arch << m_enableRigidParts;
			arch << m_bonesToDeform;
			arch << m_reshapePhysicsVolumes;
			arch << m_deformAllPhysics;
			arch << m_physicsToDeform;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver<=6);

			arch >> m_pBaseMesh;
			arch >> m_pBaseShape;
			arch >> m_pTargetShape;
			arch >> m_reshapeSkeleton;
			arch >> m_enableRigidParts;

			if (ver <= 5)
			{
				arch >> m_deformAllBones_DEPRECATED;
			}

			arch >> m_bonesToDeform;
			
		  	if (ver >= 4)
		 	{
				arch >> m_reshapePhysicsVolumes;
		 	}

			if (ver >= 5)
			{
				arch >> m_deformAllPhysics;
				arch >> m_physicsToDeform;
			}
		}

		// NodeMesh::Private interface
        Ptr<NodeLayout> GetLayout( int index ) const override;

	};

}
