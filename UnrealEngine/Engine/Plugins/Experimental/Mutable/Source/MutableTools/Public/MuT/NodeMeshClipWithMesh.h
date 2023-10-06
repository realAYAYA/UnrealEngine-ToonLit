// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
    class NodeMeshClipWithMesh;
    typedef Ptr<NodeMeshClipWithMesh> NodeMeshClipWithMeshPtr;
    typedef Ptr<const NodeMeshClipWithMesh> NodeMeshClipWithMeshConst;


    //! This node applies a geometric transform represented by a 4x4 matrix to a mesh
	//! \ingroup model
    class MUTABLETOOLS_API NodeMeshClipWithMesh : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshClipWithMesh();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshClipWithMesh* pNode, OutputArchive& arch );
        static NodeMeshClipWithMeshPtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		virtual const NODE_TYPE* GetType() const;
		static const NODE_TYPE* GetStaticType();

		virtual int GetInputCount() const;
		virtual const char* GetInputName( int i ) const;
		virtual const NODE_TYPE* GetInputType( int i ) const;
		virtual Node* GetInputNode( int i ) const;
		virtual void SetInputNode( int i, NodePtr pNode );

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Source mesh to be clipped
        NodeMeshPtr GetSource() const;
		void SetSource(NodeMesh*);

        //! \param 
		void SetClipMesh(NodeMesh*);

		//! Add a tag to the clip morph operation, which will only affect surfaces with the same tag
		void AddTag(const char* tagName);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
		Node::Private* GetBasePrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeMeshClipWithMesh();

	private:

		Private* m_pD;

	};


}
