// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
	class NodeMeshTangents;
	typedef Ptr<NodeMeshTangents> NodeMeshTangentsPtr;
	typedef Ptr<const NodeMeshTangents> NodeMeshTangentsPtrConst;


	//! This node rebuilds the tangents and binormals of the source mesh.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshTangents : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshTangents();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshTangents* pNode, OutputArchive& arch );
		static NodeMeshTangentsPtr StaticUnserialise( InputArchive& arch );

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

		//!
		NodeMeshPtr GetSource() const;
		void SetSource( NodeMesh* p );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshTangents();

	private:

		Private* m_pD;

	};


}
