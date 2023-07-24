// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

	// Forward definitions
	class NodeImageGradient;
	typedef Ptr<NodeImageGradient> NodeImageGradientPtr;
	typedef Ptr<const NodeImageGradient> NodeImageGradientPtrConst;

	class NodeColour;
	typedef Ptr<NodeColour> NodeColourPtr;
	typedef Ptr<const NodeColour> NodeColourPtrConst;


	//! This node generats an horizontal linear gradient between two colours in a new image.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageGradient : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageGradient();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageGradient* pNode, OutputArchive& arch );
		static NodeImageGradientPtr StaticUnserialise( InputArchive& arch );


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

		//! First colour of the gradient.
		NodeColourPtr GetColour0() const;
		void SetColour0( NodeColourPtr );

		//! Second colour of the gradient.
		NodeColourPtr GetColour1() const;
		void SetColour1( NodeColourPtr );

		//! Generated image size.
		int GetSizeX() const;
		int GetSizeY() const;
		void SetSize( int x, int y );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageGradient();

	private:

		Private* m_pD;

	};


}
