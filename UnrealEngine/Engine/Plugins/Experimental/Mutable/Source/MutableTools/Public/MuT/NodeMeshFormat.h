// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"

namespace mu
{

	// Forward definitions
	class NodeMeshFormat;
	typedef Ptr<NodeMeshFormat> NodeMeshFormatPtr;
	typedef Ptr<const NodeMeshFormat> NodeMeshFormatPtrConst;

	class InputArchive;
	class FMeshBufferSet;
	class OutputArchive;

	//! This node can change the buffer formats of a mesh vertices, indices and faces.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshFormat : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshFormat();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshFormat* pNode, OutputArchive& arch );
		static NodeMeshFormatPtr StaticUnserialise( InputArchive& arch );

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

		//! Source mesh to be re-formatted
		NodeMeshPtr GetSource() const;
		void SetSource( NodeMesh* );

		//! Get and set the MeshBufferSet that defines the new format for the mesh vertices. These
		//! buffers don't really contain any data (they have 0 elements) but they define the
		//! structure. If this is null, the vertex buffers will not be changed.
		FMeshBufferSet& GetVertexBuffers();

		//! Get and set the MeshBufferSet that defines the new format for the mesh indices. These
		//! buffers don't really contain any data (they have 0 elements) but they define the
		//! structure. If this is null, the vertex buffers will not be changed.
		FMeshBufferSet& GetIndexBuffers();

		//! Get and set the MeshBufferSet that defines the new format for the mesh faces. These
		//! buffers don't really contain any data (they have 0 elements) but they define the
		//! structure. If this is null, the vertex buffers will not be changed.
		FMeshBufferSet& GetFaceBuffers();

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshFormat();

	private:

		Private* m_pD;

	};


}
