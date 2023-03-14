// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	// Forward definitions
	class NodeComponent;
	typedef Ptr<NodeComponent> NodeComponentPtr;
	typedef Ptr<const NodeComponent> NodeComponentPtrConst;

    class NodeSurface;


	//! This class is the parent of all nodes that output a component.
	//! \ingroup model
	class MUTABLETOOLS_API NodeComponent : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			New = 0,
			Edit = 1,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeComponent* pNode, OutputArchive& arch );
		static NodeComponentPtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        //! \name Surfaces
        //! \{

        //! Get the number of meshes in the component.
        virtual int GetSurfaceCount() const = 0;

        //! Set the number of meshes in the component.
        virtual void SetSurfaceCount( int ) = 0;

        //! Get the node generating one of the meshes in the component.
        //! \param index index of the mesh, from 0 to GetSurfaceCount()-1
        virtual NodeSurface* GetSurface( int index ) const = 0;

        //! Set the node generating one of the meshes in the component.
        //! \param index index of the mesh, from 0 to GetSurfaceCount()-1
        virtual void SetSurface( int index, NodeSurface* ) = 0;

        //! \}

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeComponent() {}

		//!
		EType Type = EType::None;

	};


}
