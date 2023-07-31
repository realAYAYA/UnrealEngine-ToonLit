// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshBufferSet.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
	class NodeScalar;
	typedef Ptr<NodeScalar> NodeScalarPtr;
	typedef Ptr<const NodeScalar> NodeScalarPtrConst;

	class NodeMeshInterpolate;
	typedef Ptr<NodeMeshInterpolate> NodeMeshInterpolatePtr;
	typedef Ptr<const NodeMeshInterpolate> NodeMeshInterpolatePtrConst;


	//! Node that morphs (interpolates linearly) between several meshes based on a weight.
	//! The different input images are uniformly distributed in the 0 to 1 range:
	//! - if two meshes are set, the first one is 0.0 and the second one is 1.0
	//! - if three meshes are set, the first one is 0.0 the second one is 0.5 and the third one 1.0
	//! regardless of the input slots used: If B and C are set, but not A, B will be at weight 0
	//! and C will be at weight 1
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshInterpolate : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshInterpolate();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshInterpolate* pNode, OutputArchive& arch );
		static NodeMeshInterpolatePtr StaticUnserialise( InputArchive& arch );


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

		//! Get the node generating the weight of the interpolation.
		NodeScalarPtr GetFactor() const;
		void SetFactor( NodeScalarPtr );

		//! Set the number of target meshes. It will keep the currently set targets and initialise
		//! the new ones as 0.
		void SetTargetCount( int );
		int GetTargetCount() const;

		//! Get the node generating the several mesh targets of the interpolation.
		NodeMeshPtr GetTarget( int t ) const;
		void SetTarget( int t, NodeMeshPtr );

		//! Set the number of channels of the mesh that will be interpolated. It will keep the
		//! currently set targets and initialise the new ones as null. The channels that are not
		//! interpolated will be copied from the first target.
		void SetChannelCount( int );

		//! Set a channel to interpolate from the mesh.
		void SetChannel( int index, MESH_BUFFER_SEMANTIC semantic, int semanticIndex );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshInterpolate();

	private:

		Private* m_pD;

	};


}
