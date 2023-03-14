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

	class NodeImageTransform;
	typedef Ptr<NodeImageTransform> NodeImageTransformPtr;
	typedef Ptr<const NodeImageTransform> NodeImageTransformPtrConst;

	class InputArchive;
	class OutputArchive;


	//! Node that multiplies the colors of an image, channel by channel.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageTransform : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageTransform();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageTransform* pNode, OutputArchive& arch );
		static NodeImageTransformPtr StaticUnserialise( InputArchive& arch );


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

		//! Base image to multiply.
		NodeImagePtr GetBase() const;
		void SetBase( NodeImagePtr );
	
		NodeScalarPtr GetOffsetX() const;
		NodeScalarPtr GetOffsetY() const;
		NodeScalarPtr GetScaleX() const;
		NodeScalarPtr GetScaleY() const;
		NodeScalarPtr GetRotation() const;
		void SetOffsetX( NodeScalarPtr pNode );
		void SetOffsetY( NodeScalarPtr pNode );
		void SetScaleX( NodeScalarPtr pNode );
		void SetScaleY( NodeScalarPtr pNode );
		void SetRotation( NodeScalarPtr pNode );
		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageTransform();

	private:

		Private* m_pD;

	};


}
