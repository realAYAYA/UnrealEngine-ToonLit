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

	class NodeColourFromScalars;
	typedef Ptr<NodeColourFromScalars> NodeColourFromScalarsPtr;
	typedef Ptr<const NodeColourFromScalars> NodeColourFromScalarsPtrConst;


	//! Obtain a colour by sampling an image at specific homogeneous coordinates.
	//! \ingroup model
	class MUTABLETOOLS_API NodeColourFromScalars : public NodeColour
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeColourFromScalars();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeColourFromScalars* pNode, OutputArchive& arch );
		static NodeColourFromScalarsPtr StaticUnserialise( InputArchive& arch );


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
		//! If it is not specified, a value of 1 will be used.
		NodeScalarPtr GetX() const;
		void SetX( NodeScalarPtr );

		NodeScalarPtr GetY() const;
		void SetY( NodeScalarPtr );

		NodeScalarPtr GetZ() const;
		void SetZ( NodeScalarPtr );

		NodeScalarPtr GetW() const;
		void SetW( NodeScalarPtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeColourFromScalars();

	private:

		Private* m_pD;

	};


}
