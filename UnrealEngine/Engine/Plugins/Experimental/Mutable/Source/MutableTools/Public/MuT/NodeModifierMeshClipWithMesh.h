// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeModifier.h"


namespace mu
{

	// Forward definitions
	class NodeModifierMeshClipWithMesh;
	typedef Ptr<NodeModifierMeshClipWithMesh> NodeModifierMeshClipWithMeshPtr;
	typedef Ptr<const NodeModifierMeshClipWithMesh> NodeModifierMeshClipWithMeshPtrConst;

	class NodeMesh;
	typedef Ptr<NodeMesh> NodeMeshPtr;
	typedef Ptr<const NodeMesh> NodeMeshPtrConst;


	//! This node makes a new component from several meshes and images.
	//! \ingroup model
	class MUTABLETOOLS_API NodeModifierMeshClipWithMesh : public NodeModifier
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeModifierMeshClipWithMesh();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeModifierMeshClipWithMesh* pNode, OutputArchive& arch );
		static NodeModifierMeshClipWithMeshPtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

        

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        int GetInputCount() const override;
        Node* GetInputNode( int i ) const override;
        void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
        // Own interface
		//-----------------------------------------------------------------------------------------

		//! \param 
		void SetClipMesh(NodeMesh*);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeModifierMeshClipWithMesh();

	private:

		Private* m_pD;

	};


}
