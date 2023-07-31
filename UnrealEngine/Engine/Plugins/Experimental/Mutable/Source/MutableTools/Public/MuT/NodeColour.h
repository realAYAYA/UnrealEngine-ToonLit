// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	class NodeColour;
	typedef Ptr<NodeColour> NodeColourPtr;
	typedef Ptr<const NodeColour> NodeColourPtrConst;


    //! %Base class of any node that outputs a colour.
    //! \ingroup model
    class MUTABLETOOLS_API NodeColour : public Node
    {
    public:

		// Possible subclasses
		enum class EType : uint8
		{
			Constant = 0,
			Parameter = 1,
			SampleImage = 2,
			Table = 3,
			ImageSize = 4,
			FromScalars = 5,
			ArithmeticOperation = 6,
			Switch = 7,
			Variation = 8,

			None
		};

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        static void Serialise( const NodeColour* pNode, OutputArchive& arch );
        static NodeColourPtr StaticUnserialise( InputArchive& arch );


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
        inline ~NodeColour() {}

		//!
		EType Type = EType::None;

    };

}

