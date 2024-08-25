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


	/** Node that morphs a base mesh with one weighted target. */
	class MUTABLETOOLS_API NodeMeshMorph : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshMorph();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshMorph* pNode, OutputArchive& arch );
		static Ptr<NodeMeshMorph> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------		

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		/** Get the node generating the weight used to select(and combine) the morphs to apply. */
		Ptr<NodeScalar> GetFactor() const;
		void SetFactor( Ptr<NodeScalar> );

		/** Get the nodes generating the base mesh to be morphed. */
		Ptr<NodeMesh> GetBase() const;
		void SetBase( Ptr<NodeMesh> );

		/** Get the nodes generating the several morph targets. */
		Ptr<NodeMesh> GetMorph() const;
		void SetMorph( Ptr<NodeMesh> );

		/** Also deform the mesh skeleton. Disabled by default. */
		void SetReshapeSkeleton(bool);	

		/** Deform Mesh Physics Volumes */
		void SetReshapePhysicsVolumes(bool);

		/** Sets the number of bones that will be deform */
		void AddBoneToDeform(const uint16 BoneName);	

		/** Add a Physics Body to deform */
		void AddPhysicsBodyToDeform(const uint16 BoneName);

        //-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshMorph();

	private:

		Private* m_pD;

	};


}
