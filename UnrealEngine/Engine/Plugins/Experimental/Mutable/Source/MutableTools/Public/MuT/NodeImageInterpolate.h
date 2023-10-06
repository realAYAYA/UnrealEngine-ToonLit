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

	class NodeImageInterpolate;
	typedef Ptr<NodeImageInterpolate> NodeImageInterpolatePtr;
	typedef Ptr<const NodeImageInterpolate> NodeImageInterpolatePtrConst;


	//! Node that interpolates linearly between several images based on a weight.
	//! The different input images are uniformly distributed in the 0 to 1 range:
	//! - if two images are set, the first one is 0.0 and the second one is 1.0
	//! - if three images are set, the first one is 0.0 the second one is 0.5 and the third one 1.0
	//! regardless of the input slots used: If B and C are set, but not A, B will be at weight 0
	//! and C will be at weight 1
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageInterpolate : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageInterpolate();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageInterpolate* pNode, OutputArchive& arch );
		static NodeImageInterpolatePtr StaticUnserialise( InputArchive& arch );


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

		//! Get the node generating the weight of the interpolation.
		NodeScalarPtr GetFactor() const;
		void SetFactor( NodeScalarPtr );

		//! Set the number of target images. It will keep the currently set targets and initialise
		//! the new ones as null.
		void SetTargetCount( int );
		int GetTargetCount() const;

		//! Get the node generating the t-th target image of the interpolation.
		NodeImagePtr GetTarget( int t ) const;
		void SetTarget( int t, NodeImagePtr );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageInterpolate();

	private:

		Private* m_pD;

	};


}
