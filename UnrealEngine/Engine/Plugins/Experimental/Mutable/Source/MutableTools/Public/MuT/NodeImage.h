// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"

namespace mu
{

	// Forward definitions
	class NodeImage;
	typedef Ptr<NodeImage> NodeImagePtr;
	typedef Ptr<const NodeImage> NodeImagePtrConst;


    //! %Base class of any node that outputs an image.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImage : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			Constant = 1,
			Difference = 2,
			Interpolate = 4,
			Saturate = 6,
			Table = 9,
			Swizzle = 10,
			SelectColour = 11,
			ColourMap = 12,
			Gradient = 13,
			VolumeLayer_Deprecated = 14,
			Binarise = 17,
			Luminance = 18,
			Layer = 19,
			LayerColour = 20,
			Resize = 21,
			PlainColour = 22,
			Interpolate3 = 23,
			Project = 24,
			Mipmap = 25,
			Switch = 26,
			Conditional = 27,
			Format = 28,
			Parameter = 29,
			MultiLayer = 30,
			Invert = 31,
			Variation = 32,
			NormalComposite = 33,
			Transform = 34,
			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeImage* pNode, OutputArchive& arch );
		static NodeImagePtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		inline EType GetImageNodeType() const { return Type; }


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeImage() {}

		//!
		EType Type = EType::None;

	};



}
