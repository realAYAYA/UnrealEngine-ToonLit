// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"
#include "MuR/Mesh.h"

namespace mu
{

	//! Node that morphs a base mesh with one or two weighted targets from a sequence.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshReshape : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshReshape();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshReshape* pNode, OutputArchive& arch );
		static Ptr<NodeMeshReshape> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		int GetInputCount() const override;
		Node* GetInputNode( int i ) const override;
		void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		const NodeMeshPtr& GetBaseMesh() const;
		void SetBaseMesh( const NodeMeshPtr& );

		const NodeMeshPtr& GetBaseShape() const;
		void SetBaseShape(const NodeMeshPtr&);

		const NodeMeshPtr& GetTargetShape() const;
		void SetTargetShape(const NodeMeshPtr&);

		void SetReshapeVertices(bool);

		/** Also deform the mesh skeleton. Disabled by default. */
		void SetReshapeSkeleton(bool);
		
		/** Set vertex color channel usages for Reshape operations. */
		void SetColorUsages(EVertexColorUsage R, EVertexColorUsage G, EVertexColorUsage B, EVertexColorUsage A);

		/** Deform Mesh Physics Volumes */
		void SetReshapePhysicsVolumes(bool);

		/** Sets the number of bones that will be deform */
		void AddBoneToDeform(const uint16 BoneId);
	
		/** Add a Physics Body to deform */
		void AddPhysicsBodyToDeform(const uint16 BoneId);
        
		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshReshape();

	private:

		Private* m_pD;

	};

}
