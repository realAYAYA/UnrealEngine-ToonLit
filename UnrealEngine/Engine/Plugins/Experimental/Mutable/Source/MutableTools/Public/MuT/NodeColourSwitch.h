// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"


namespace mu
{

	// Forward definitions
	class NodeScalar;
	typedef Ptr<NodeScalar> NodeScalarPtr;
	typedef Ptr<const NodeScalar> NodeScalarPtrConst;

    class NodeColourSwitch;
    typedef Ptr<NodeColourSwitch> NodeColourSwitchPtr;
    typedef Ptr<const NodeColourSwitch> NodeColourSwitchPtrConst;


    //! This node selects an output Colour from a set of input Colours based on a parameter.
	//! \ingroup model
    class MUTABLETOOLS_API NodeColourSwitch : public NodeColour
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeColourSwitch();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeColourSwitch* pNode, OutputArchive& arch );
        static NodeColourSwitchPtr StaticUnserialise( InputArchive& arch );


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
		NodeScalarPtr GetParameter() const;
		void SetParameter( NodeScalarPtr );

        //! Set the number of option Colours. It will keep the currently set targets and initialise
		//! the new ones as null.
		void SetOptionCount( int );

        //! Get the node generating the t-th option Colour.
        NodeColourPtr GetOption( int t ) const;
        void SetOption( int t, NodeColourPtr );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeColourSwitch();

	private:

		Private* m_pD;

	};


}
