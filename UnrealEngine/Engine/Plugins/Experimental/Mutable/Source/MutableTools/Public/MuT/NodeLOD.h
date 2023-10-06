// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	// Forward definitions
	class NodeComponent;
	typedef Ptr<NodeComponent> NodeComponentPtr;
	typedef Ptr<const NodeComponent> NodeComponentPtrConst;

	class NodeModifier;
	typedef Ptr<NodeModifier> NodeModifierPtr;
	typedef Ptr<const NodeModifier> NodeModifierPtrConst;

	class NodeLOD;
	typedef Ptr<NodeLOD> NodeLODPtr;
	typedef Ptr<const NodeLOD> NodeLODPtrConst;


	//! Node that creates a new level of detail by assembling several object components.
	//! \ingroup model
	class MUTABLETOOLS_API NodeLOD : public Node
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeLOD();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeLOD* pNode, OutputArchive& arch );
		static NodeLODPtr StaticUnserialise( InputArchive& arch );


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

		//! Get the number of components in the LOD to create
		int GetComponentCount() const;

		//! Set the number of components in the LOD to create
		void SetComponentCount( int );

		//! Get the node generating one of the components of the LOD.
		//! \param index index of the component from 0 to GetComponentCount()-1
		NodeComponentPtr GetComponent( int index ) const;

		//! Set the node generating one of the components of the LOD.
		//! \param index index of the component from 0 to GetComponentCount()-1
		void SetComponent( int index, NodeComponentPtr );

		//! Set the number of modifiers in the LOD
		void SetModifierCount(int);

		//! Get the number of modifiers in the LOD
		int GetModifierCount() const;

		//! Set the node generating one of the modifiers of the LOD.
		//! \param index index of the component from 0 to GetModifierCount()-1
		void SetModifier(int index, NodeModifierPtr);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeLOD();

	private:

		Private* m_pD;

	};



}
