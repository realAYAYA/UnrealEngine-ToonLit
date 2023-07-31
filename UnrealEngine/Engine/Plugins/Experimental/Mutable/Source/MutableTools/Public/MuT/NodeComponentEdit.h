// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeComponent.h"


namespace mu
{

	// Forward definitions
	class NodeComponentEdit;
	typedef Ptr<NodeComponentEdit> NodeComponentEditPtr;
	typedef Ptr<const NodeComponentEdit> NodeComponentEditPtrConst;

    class NodeSurface;
    typedef Ptr<NodeSurface> NodeSurfacePtr;
    typedef Ptr<const NodeSurface> NodeSurfacePtrConst;



	//! This node modifies a node of the parent object of the object that this node belongs to.
	//! It allows to extend, cut and morph the parent component's meshes.
	//! It also allows to patch the parent component's textures.
	//! \ingroup model
	class MUTABLETOOLS_API NodeComponentEdit : public NodeComponent
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeComponentEdit();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeComponentEdit* pNode, OutputArchive& arch );
		static NodeComponentEditPtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------
		

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		int GetInputCount() const override;
		Node* GetInputNode( int i ) const override;
		void SetInputNode( int i, NodePtr pNode ) override;

        //-----------------------------------------------------------------------------------------
        // NodeComponent interface
        //-----------------------------------------------------------------------------------------
        int GetSurfaceCount() const override;
        void SetSurfaceCount( int ) override;
        NodeSurface* GetSurface( int index ) const override;
        void SetSurface( int index, NodeSurface* ) override;

		//-----------------------------------------------------------------------------------------
        // Own interface
		//-----------------------------------------------------------------------------------------

		//! Set the parent component to modify. It should be a node from the same LOD of one of
		//! the parent objects of this node's object.
        void SetParent( NodeComponent* );
        NodeComponent* GetParent() const;


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeComponentEdit();

	private:

		Private* m_pD;

	};



}
