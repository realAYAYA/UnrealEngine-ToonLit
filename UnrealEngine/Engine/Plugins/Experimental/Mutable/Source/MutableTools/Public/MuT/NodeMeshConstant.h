// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Mesh.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
	class NodeMeshConstant;
	typedef Ptr<NodeMeshConstant> NodeMeshConstantPtr;
	typedef Ptr<const NodeMeshConstant> NodeMeshConstantPtrConst;

	class NodeLayout;
	typedef Ptr<NodeLayout> NodeLayoutPtr;
	typedef Ptr<const NodeLayout> NodeLayoutPtrConst;


	//! Node that outputs a constant mesh.
	//! It allows to define the layouts for the texture channels of the constant mesh
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshConstant : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshConstant();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshConstant* pNode, OutputArchive& arch );
		static NodeMeshConstantPtr StaticUnserialise( InputArchive& arch );


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

		//! Get the constant mesh that will be returned.
		MeshPtr GetValue() const;

		//! Set the constant mesh that will be returned.
		void SetValue( MeshPtr );

		//! Get the number of layouts defined in this mesh.
		int GetLayoutCount() const;
		void SetLayoutCount( int );

		//! Get the node defining a layout of the returned mesh.
		NodeLayoutPtr GetLayout( int index ) const;
		void SetLayout( int index, NodeLayoutPtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshConstant();

	private:

		Private* m_pD;

	};


}
