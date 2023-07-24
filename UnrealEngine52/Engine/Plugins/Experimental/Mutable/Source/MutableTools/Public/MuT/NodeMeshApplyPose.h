// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
    class NodeMeshApplyPose;
    using NodeMeshApplyPosePtr = Ptr<NodeMeshApplyPose> ;
    using NodeMeshApplyPosePtrConst = Ptr<const NodeMeshApplyPose>;


    //! Node that applies a pose to a mesh, baking it into the vertex data
	//! \ingroup model
    class MUTABLETOOLS_API NodeMeshApplyPose : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeMeshApplyPose();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshApplyPose* pNode, OutputArchive& arch );
        static NodeMeshApplyPosePtr StaticUnserialise( InputArchive& arch );


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

		//! Get the nodes generating the base mesh to be morphed
		NodeMeshPtr GetBase() const;
		void SetBase( NodeMeshPtr );

        //! Get the nodes generating the pose to apply. A pose is represented with a mesh object
        //! with no geometry: only bone matrices and names are relevant.
        NodeMeshPtr GetPose() const;
        void SetPose( NodeMeshPtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeMeshApplyPose();

	private:

		Private* m_pD;

	};


}
