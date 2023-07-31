// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

	// Forward definitions
	class NodeColour;
	typedef Ptr<NodeColour> NodeColourPtr;
	typedef Ptr<const NodeColour> NodeColourPtrConst;

	class NodeImageSelectColour;
	typedef Ptr<NodeImageSelectColour> NodeImageSelectColourPtr;
	typedef Ptr<const NodeImageSelectColour> NodeImageSelectColourPtrConst;

	class InputArchive;
	class OutputArchive;


	//! Create a black and white mask image by selecting the pixels from a source image that match
	//! a colour.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageSelectColour : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageSelectColour();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageSelectColour* pNode, OutputArchive& arch );
		static NodeImageSelectColourPtr StaticUnserialise( InputArchive& arch );


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

		//! Get the node generating the colour to select.
		NodeColourPtr GetColour() const;
		void SetColour( NodeColourPtr );

		//! Get the node generating the source image to select pixels from.
		NodeImagePtr GetSource() const;
		void SetSource( NodeImagePtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageSelectColour();

	private:

		Private* m_pD;

	};


}
