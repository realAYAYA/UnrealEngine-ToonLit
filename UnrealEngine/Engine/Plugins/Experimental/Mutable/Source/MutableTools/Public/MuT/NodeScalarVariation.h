// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"


namespace mu
{

    // Forward definitions
    class NodeScalarVariation;
    typedef Ptr<NodeScalarVariation> NodeScalarVariationPtr;
    typedef Ptr<const NodeScalarVariation> NodeScalarVariationPtrConst;


    //!
    //! \ingroup model
    class MUTABLETOOLS_API NodeScalarVariation : public NodeScalar
    {
    public:
        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        NodeScalarVariation();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeScalarVariation* pNode, OutputArchive& arch );
        static NodeScalarVariationPtr StaticUnserialise( InputArchive& arch );

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
        void SetDefaultScalar( NodeScalar* Scalar );

        //! Set the number of tags to consider in this variation
        void SetVariationCount( int count );

        //!
        int GetVariationCount() const;

        //! Set the tag or state name that will enable a specific vartiation
        void SetVariationTag( int index, const char* strTag );

        //!
        void SetVariationScalar( int index, NodeScalar* Scalar );

        //!}


        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;
        Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

    protected:
        //! Forbidden. Manage with the Ptr<> template.
        ~NodeScalarVariation();

    private:
        Private* m_pD;
    };


} // namespace mu
