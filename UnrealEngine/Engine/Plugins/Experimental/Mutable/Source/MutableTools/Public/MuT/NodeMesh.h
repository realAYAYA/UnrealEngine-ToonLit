// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"

namespace mu
{

	// Forward definitions
	class NodeMesh;
	typedef Ptr<NodeMesh> NodeMeshPtr;
	typedef Ptr<const NodeMesh> NodeMeshPtrConst;


    //! %Base class of any node that outputs a mesh.
	//! \ingroup model
	class MUTABLETOOLS_API NodeMesh : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			Constant = 0,
			Interpolate = 2,
			Table = 4,
			Subtract = 6,
			Format = 7,
			Tangents = 8,
			Morph = 9,
			MakeMorph = 10,
			Switch = 11,
			Fragment = 12,
			Transform = 13,
			ClipMorphPlane = 14,
			ClipWithMesh = 15,
			ApplyPose = 16,
			Variation = 17,
			GeometryOperation = 18,
			Reshape = 19,
			ClipDeform = 20,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeMesh* pNode, OutputArchive& arch );
		static NodeMeshPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		inline EType GetMeshNodeType() const { return Type; }


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeMesh() {}

		//!
		EType Type = EType::None;
	};


}

