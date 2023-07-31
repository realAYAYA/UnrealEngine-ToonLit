// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
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

	class NodeImageInterpolate3;
	typedef Ptr<NodeImageInterpolate3> NodeImageInterpolate3Ptr;
	typedef Ptr<const NodeImageInterpolate3> NodeImageInterpolate3PtrConst;


	//! Node that Interpolates linearly between 3 images. It has two weights and the third one is
	//! deduced as 1-w0-w1.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageInterpolate3 : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageInterpolate3();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageInterpolate3* pNode, OutputArchive& arch );
		static NodeImageInterpolate3Ptr StaticUnserialise( InputArchive& arch );


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

		//! Get the weight of the first target. If this is 1 (so the other is 0) the resulting
		//! image is the target 1. If both weights are 0, the result is target 0.
		NodeScalarPtr GetFactor1() const;
		void SetFactor1( NodeScalarPtr );

		//! Get the weight of the second target. If this is 1 (so the other is 1) the resulting
		//! image is the target 2. If both weights are 0, the result is target 0.
		NodeScalarPtr GetFactor2() const;
		void SetFactor2( NodeScalarPtr );

		//! Get the node generating the first target image of the interpolation.
		NodeImagePtr GetTarget0() const;
		void SetTarget0( NodeImagePtr );

		//! Get the node generating the second target image of the interpolation.
		NodeImagePtr GetTarget1() const;
		void SetTarget1( NodeImagePtr );

		//! Get the node generating the third target image of the interpolation.
		NodeImagePtr GetTarget2() const;
		void SetTarget2( NodeImagePtr );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageInterpolate3();

	private:

		Private* m_pD;

	};


}
