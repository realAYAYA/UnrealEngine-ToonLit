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

	class NodeImage;
	typedef Ptr<NodeImage> NodeImagePtr;
	typedef Ptr<const NodeImage> NodeImagePtrConst;

	class NodeColourSampleImage;
	typedef Ptr<NodeColourSampleImage> NodeColourSampleImagePtr;
	typedef Ptr<const NodeColourSampleImage> NodeColourSampleImagePtrConst;


	//! Obtain a colour by sampling an image at specific homogeneous coordinates.
	//! \ingroup model
	class MUTABLETOOLS_API NodeColourSampleImage : public NodeColour
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeColourSampleImage();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeColourSampleImage* pNode, OutputArchive& arch );
		static NodeColourSampleImagePtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		int GetInputCount() const override;
		Node* GetInputNode( int i ) const override;
		void SetInputNode( int i, NodePtr pNode ) override;


		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the node generating the x coordinate to sample, in the range of 0.0 to 1.0.
		//! If it is not specified, a value of x =0.5 will be used.
		NodeScalarPtr GetX() const;
		void SetX( NodeScalarPtr );

		//! Get the node generating the y coordinate to sample, in the range of 0.0 to 1.0.
		//! If it is not specified, a value of y =0.5 will be used.
		NodeScalarPtr GetY() const;
		void SetY( NodeScalarPtr );

		//! Get the node generating the source image to be sampled.
		NodeImagePtr GetImage() const;
		void SetImage( NodeImagePtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeColourSampleImage();

	private:

		Private* m_pD;

	};


}
