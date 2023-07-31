// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeComponent.h"


namespace mu
{

	// Forward definitions
	class NodeComponent;
	typedef Ptr<NodeComponent> NodeComponentPtr;
	typedef Ptr<const NodeComponent> NodeComponentPtrConst;

	class NodeComponentNew;
	typedef Ptr<NodeComponentNew> NodeComponentNewPtr;
	typedef Ptr<const NodeComponentNew> NodeComponentNewPtrConst;

    class NodeSurface;
    typedef Ptr<NodeSurface> NodeSurfacePtr;
    typedef Ptr<const NodeSurface> NodeSurfacePtrConst;


	//! This node makes a new component from several meshes and images.
	//! \ingroup model
	class MUTABLETOOLS_API NodeComponentNew : public NodeComponent
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeComponentNew();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeComponentNew* pNode, OutputArchive& arch );
		static NodeComponentNewPtr StaticUnserialise( InputArchive& arch );

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

		//! Get the name of the component.
		const char* GetName() const;

		//! Set the name of the component.
		void SetName( const char* );

		//! Get the id of the component.
		uint16 GetId() const;

		//! Set the id of the component.
		void SetId( uint16 );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeComponentNew();

	private:

		Private* m_pD;

	};


}
