// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	// Forward definitions
    class NodeMeshMakeMorph;
    typedef Ptr<NodeMeshMakeMorph> NodeMeshMakeMorphPtr;
    typedef Ptr<const NodeMeshMakeMorph> NodeMeshMakeMorphPtrConst;


	//! This node removes from a mesh A all the faces that are also part of a mesh B.
	//! \ingroup model
    class MUTABLETOOLS_API NodeMeshMakeMorph : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeMeshMakeMorph();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshMakeMorph* pNode, OutputArchive& arch );
        static NodeMeshMakeMorphPtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//!
		NodeMeshPtr GetBase() const;
		void SetBase( NodeMesh* p );

		//!
		NodeMeshPtr GetTarget() const;
		void SetTarget( NodeMesh* p );

		bool GetOnlyPositionAndNormal() const;
		void SetOnlyPositionAndNormal(bool bOnlyPositionAndNormal);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeMeshMakeMorph();

	private:

		Private* m_pD;

	};


}
