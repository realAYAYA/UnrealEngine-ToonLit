// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

	// Forward definitions
	class NodeImageDifference;
	typedef Ptr<NodeImageDifference> NodeImageDifferencePtr;
	typedef Ptr<const NodeImageDifference> NodeImageDifferencePtrConst;

	class InputArchive;
	class OutputArchive;


	//! Node that compares two images and returns a black-and-white image with pixels that are
	//! "significally different", meaning around 0.03 different. This node is for testing
	//! purposes.
	//! \todo rename to NodeImageCompare
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageDifference : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageDifference();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageDifference* pNode, OutputArchive& arch );
		static NodeImageDifferencePtr StaticUnserialise( InputArchive& arch );


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

		//! Return the first image to compare.
		NodeImagePtr GetA() const;

		//! Set the first image to compare.
		void SetA( NodeImagePtr );

		//! Return the second image to compare.
		NodeImagePtr GetB() const;

		//! Set the second image to compare.
		void SetB( NodeImagePtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageDifference();

	private:

		Private* m_pD;

	};


}
