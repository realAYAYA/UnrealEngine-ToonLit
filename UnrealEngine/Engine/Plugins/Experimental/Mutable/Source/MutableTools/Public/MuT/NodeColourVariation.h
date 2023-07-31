// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"


namespace mu
{

    // Forward definitions
    class NodeColourVariation;
    typedef Ptr<NodeColourVariation> NodeColourVariationPtr;
    typedef Ptr<const NodeColourVariation> NodeColourVariationPtrConst;


    //!
    //! \ingroup model
    class MUTABLETOOLS_API NodeColourVariation : public NodeColour
    {
    public:
        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        NodeColourVariation();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeColourVariation* pNode, OutputArchive& arch );
        static NodeColourVariationPtr StaticUnserialise( InputArchive& arch );

        //-----------------------------------------------------------------------------------------
        // Node Interface
        //-----------------------------------------------------------------------------------------

        

        const NODE_TYPE* GetType() const override;
        static const NODE_TYPE* GetStaticType();

        virtual int GetInputCount() const override;
        virtual Node* GetInputNode( int i ) const override;
        void SetInputNode( int i, NodePtr pNode ) override;

        //-----------------------------------------------------------------------------------------
        // Own Interface
        //-----------------------------------------------------------------------------------------

        //!
        void SetDefaultColour( NodeColour* Colour );

        //! Set the number of tags to consider in this variation
        void SetVariationCount( int count );

        //!
        int GetVariationCount() const;

        //! Set the tag or state name that will enable a specific vartiation
        void SetVariationTag( int index, const char* strTag );

        //!
        void SetVariationColour( int index, NodeColour* Colour );

        //!}


        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;
        Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

    protected:
        //! Forbidden. Manage with the Ptr<> template.
        ~NodeColourVariation();

    private:
        Private* m_pD;
    };


} // namespace mu
