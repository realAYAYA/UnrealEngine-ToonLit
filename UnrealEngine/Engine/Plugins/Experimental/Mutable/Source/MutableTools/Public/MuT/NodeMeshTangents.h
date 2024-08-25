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

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

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
