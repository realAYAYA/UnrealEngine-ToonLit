// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

	// Forward definitions
	class NodeScalar;
	typedef Ptr<NodeScalar> NodeScalarPtr;
	typedef Ptr<const NodeScalar> NodeScalarPtrConst;

	class NodeImageSwitch;
	typedef Ptr<NodeImageSwitch> NodeImageSwitchPtr;
	typedef Ptr<const NodeImageSwitch> NodeImageSwitchPtrConst;


	//! This node selects an output image from a set of input images based on a parameter.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageSwitch : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageSwitch();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageSwitch* pNode, OutputArchive& arch );
		static NodeImageSwitchPtr StaticUnserialise( InputArchive& arch );


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

		//! Set the number of option images. It will keep the currently set targets and initialise
		//! the new ones as null.
		void SetOptionCount( int );
		int GetOptionCount() const;

		//! Get the node generating the t-th option image.
		NodeImagePtr GetOption( int t ) const;
		void SetOption( int t, NodeImagePtr );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageSwitch();

	private:

		Private* m_pD;

	};


}
