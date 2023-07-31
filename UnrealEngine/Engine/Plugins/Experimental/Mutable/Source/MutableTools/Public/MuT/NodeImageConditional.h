// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

	// Forward definitions
    class NodeBool;
    typedef Ptr<NodeBool> NodeBoolPtr;
    typedef Ptr<const NodeBool> NodeBoolPtrConst;

    class NodeImageConditional;
    typedef Ptr<NodeImageConditional> NodeImageConditionalPtr;
    typedef Ptr<const NodeImageConditional> NodeImageConditionalPtrConst;


	//! This node selects an output image from a set of input images based on a parameter.
	//! \ingroup model
    class MUTABLETOOLS_API NodeImageConditional : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeImageConditional();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageConditional* pNode, OutputArchive& arch );
        static NodeImageConditionalPtr StaticUnserialise( InputArchive& arch );


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

		//! Get the node generating the parameter used to select the option.
        NodeBool* GetParameter() const;
        void SetParameter( NodeBool* );

        //! Get the node generating the option image when the parameters is true.
        NodeImage* GetOptionTrue() const;
        void SetOptionTrue( NodeImage* );

        NodeImage* GetOptionFalse() const;
        void SetOptionFalse( NodeImage* );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeImageConditional();

	private:

		Private* m_pD;

	};


}
