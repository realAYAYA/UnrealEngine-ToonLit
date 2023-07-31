// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

    // Forward definitions
    class NodeImageVariation;
    typedef Ptr<NodeImageVariation> NodeImageVariationPtr;
    typedef Ptr<const NodeImageVariation> NodeImageVariationPtrConst;

	class InputArchive;
	class OutputArchive;


    //!
    //! \ingroup model
    class MUTABLETOOLS_API NodeImageVariation : public NodeImage
    {
    public:
        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        NodeImageVariation();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageVariation* pNode, OutputArchive& arch );
        static NodeImageVariationPtr StaticUnserialise( InputArchive& arch );

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
        void SetDefaultImage( NodeImage* Image );

        //! Set the number of tags to consider in this variation
        void SetVariationCount( int count );

        //!
        int GetVariationCount() const;

        //! Set the tag or state name that will enable a specific vartiation
        void SetVariationTag( int index, const char* strTag );

        //!
        void SetVariationImage( int index, NodeImage* Image );

        //!}


        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;
        Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

    protected:
        //! Forbidden. Manage with the Ptr<> template.
        ~NodeImageVariation();

    private:
        Private* m_pD;
    };


} // namespace mu
