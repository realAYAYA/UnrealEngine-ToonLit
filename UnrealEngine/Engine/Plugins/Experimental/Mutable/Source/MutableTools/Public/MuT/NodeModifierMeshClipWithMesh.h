// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeModifier.h"


namespace mu
{

	// Forward definitions
	class NodeMesh;


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
		static Ptr<NodeModifierMeshClipWithMesh> StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

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
