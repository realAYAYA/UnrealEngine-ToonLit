// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
	class NodeMeshSubtract;
	typedef Ptr<NodeMeshSubtract> NodeMeshSubtractPtr;
	typedef Ptr<const NodeMeshSubtract> NodeMeshSubtractPtrConst;


	//! This node removes from a mesh A all the faces that are also part of a mesh B.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshSubtract : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshSubtract();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshSubtract* pNode, OutputArchive& arch );
		static NodeMeshSubtractPtr StaticUnserialise( InputArchive& arch );

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
		NodeMeshPtr GetA() const;
		void SetA( NodeMesh* p );

		//!
		NodeMeshPtr GetB() const;
		void SetB( NodeMesh* p );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshSubtract();

	private:

		Private* m_pD;

	};


}
