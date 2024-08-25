// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	// Forward definitions
    class NodeSurface;
    typedef Ptr<NodeSurface> NodeSurfacePtr;
    typedef Ptr<const NodeSurface> NodeSurfacePtrConst;



    //! This class is the parent of all nodes that output a Surface.
	//! \ingroup model
    class MUTABLETOOLS_API NodeSurface : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			New = 0,
			Edit = 1,
			Variation = 3,
			Switch = 4,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        static void Serialise( const NodeSurface* pNode, OutputArchive& arch );
        static NodeSurfacePtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();


	protected:

		//! Forbidden. Manage with the Ptr<> template.
        inline ~NodeSurface() {}

		//!
		EType Type = EType::None;

	};


}
