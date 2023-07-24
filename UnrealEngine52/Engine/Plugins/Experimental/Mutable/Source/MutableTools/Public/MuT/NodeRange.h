// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

    // Forward definitions
    class NodeRange;
    typedef Ptr<NodeRange> NodeRangePtr;
    typedef Ptr<const NodeRange> NodeRangePtrConst;


    //! %Base class of any node that outputs a range.
	//! \ingroup model
    class MUTABLETOOLS_API NodeRange : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			FromScalar = 0,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        static void Serialise( const NodeRange* pNode, OutputArchive& arch );
        static NodeRangePtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        inline ~NodeRange() {}

		//!
		EType Type = EType::None;

	};


}

