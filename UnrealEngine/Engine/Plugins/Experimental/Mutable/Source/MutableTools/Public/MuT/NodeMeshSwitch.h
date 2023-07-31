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

	class NodeMeshSwitch;
	typedef Ptr<NodeMeshSwitch> NodeMeshSwitchPtr;
	typedef Ptr<const NodeMeshSwitch> NodeMeshSwitchPtrConst;


	//! This node selects an output Mesh from a set of input Meshs based on a parameter.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMeshSwitch : public NodeMesh
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeMeshSwitch();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeMeshSwitch* pNode, OutputArchive& arch );
		static NodeMeshSwitchPtr StaticUnserialise( InputArchive& arch );


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

		//! Get the node generating the parameter used to select the option.
		NodeScalarPtr GetParameter() const;
		void SetParameter( NodeScalarPtr );

		//! Set the number of option Meshs. It will keep the currently set targets and initialise
		//! the new ones as null.
		void SetOptionCount( int );

		//! Get the node generating the t-th option Mesh.
		NodeMeshPtr GetOption( int t ) const;
		void SetOption( int t, NodeMeshPtr );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshSwitch();

	private:

		Private* m_pD;

	};


}
