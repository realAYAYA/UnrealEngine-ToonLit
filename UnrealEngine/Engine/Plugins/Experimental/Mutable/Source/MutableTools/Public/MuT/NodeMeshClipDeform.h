// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	//! Node that morphs a base mesh with one or two weighted targets from a sequence.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshClipDeform : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshClipDeform();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshClipDeform* pNode, OutputArchive& arch );
		static Ptr<NodeMeshClipDeform> StaticUnserialise( InputArchive& arch );


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

		const NodeMeshPtr& GetClipShape() const;
		void SetClipShape(const NodeMeshPtr&);

		const NodeImagePtr& GetShapeWeights() const;
		void SetShapeWeights(const NodeImagePtr&);


        //-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshClipDeform();

	private:

		Private* m_pD;

	};

}
