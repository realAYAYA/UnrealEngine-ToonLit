// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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

	class NodeMeshGeometryOperation;
	typedef Ptr<NodeMeshGeometryOperation> NodeMeshGeometryOperationPtr;
	typedef Ptr<const NodeMeshGeometryOperation> NodeMeshGeometryOperationPtrConst;


	//! Node that morphs a base mesh with one or two weighted targets from a sequence.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshGeometryOperation : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshGeometryOperation();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshGeometryOperation* pNode, OutputArchive& arch );
		static NodeMeshGeometryOperationPtr StaticUnserialise( InputArchive& arch );


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

		const NodeMeshPtr& GetMeshA() const;
		void SetMeshA( const NodeMeshPtr& );

		const NodeMeshPtr& GetMeshB() const;
		void SetMeshB(const NodeMeshPtr&);

		const NodeScalarPtr& GetScalarA() const;
		void SetScalarA(const NodeScalarPtr&);

		const NodeScalarPtr& GetScalarB() const;
		void SetScalarB(const NodeScalarPtr&);

        //-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshGeometryOperation();

	private:

		Private* m_pD;

	};

}
