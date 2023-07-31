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

	class NodeImagePlainColour;
	typedef Ptr<NodeImagePlainColour> NodeImagePlainColourPtr;
	typedef Ptr<const NodeImagePlainColour> NodeImagePlainColourPtrConst;

	class InputArchive;
	class OutputArchive;


	//! Node that multiplies the colors of an image, channel by channel.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImagePlainColour : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImagePlainColour();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImagePlainColour* pNode, OutputArchive& arch );
		static NodeImagePlainColourPtr StaticUnserialise( InputArchive& arch );


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

		//! Colour of the image.
		NodeColourPtr GetColour() const;
		void SetColour( NodeColourPtr );

		//! New size or relative factor
		void SetSize( int x, int y );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImagePlainColour();

	private:

		Private* m_pD;

	};


}
