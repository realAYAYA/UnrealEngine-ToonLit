// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"

namespace mu
{

	// Forward definitions
	class NodePatchMesh;
	typedef Ptr<NodePatchMesh> NodePatchMeshPtr;
	typedef Ptr<const NodePatchMesh> NodePatchMeshPtrConst;

	class NodeMesh;

	//!
	//! \ingroup model
	class MUTABLETOOLS_API NodePatchMesh : public Node
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodePatchMesh();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodePatchMesh* pNode, OutputArchive& arch );
		static NodePatchMeshPtr StaticUnserialise( InputArchive& arch );

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
        NodeMesh* GetRemove() const;
        void SetRemove( NodeMesh* );

		//!
        NodeMesh* GetAdd() const;
        void SetAdd( NodeMesh* );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodePatchMesh();

	private:

		Private* m_pD;

	};


}
