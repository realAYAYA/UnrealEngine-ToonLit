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

	class NodeImageSaturate;
	typedef Ptr<NodeImageSaturate> NodeImageSaturatePtr;
	typedef Ptr<const NodeImageSaturate> NodeImageSaturatePtrConst;

	class InputArchive;
	class OutputArchive;


	//! Change the saturation of an image. This node can be used to increase the saturation with a
	//! factor bigger than 1, or to decrease it or desaturate it completely with a factor smaller
	//! than 1 or 0.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageSaturate : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageSaturate();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageSaturate* pNode, OutputArchive& arch );
		static NodeImageSaturatePtr StaticUnserialise( InputArchive& arch );


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

		//! Get the node generating the saturation factor.
		//! A value of 0 completely desaturates. 1 will leave the same saturation and bigger than 1
		//! will increase it.
		NodeScalarPtr GetFactor() const;
		void SetFactor( NodeScalarPtr );

		//! Get the node generating the source image to be [de]saturated.
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
		~NodeImageSaturate();

	private:

		Private* m_pD;

	};


}
