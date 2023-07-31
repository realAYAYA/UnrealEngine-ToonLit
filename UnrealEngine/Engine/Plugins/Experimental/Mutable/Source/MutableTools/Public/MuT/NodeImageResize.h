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

	class NodeImageResize;
	typedef Ptr<NodeImageResize> NodeImageResizePtr;
	typedef Ptr<const NodeImageResize> NodeImageResizePtrConst;

	class InputArchive;
	class OutputArchive;


	//! Node that multiplies the colors of an image, channel by channel.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageResize : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageResize();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageResize* pNode, OutputArchive& arch );
		static NodeImageResizePtr StaticUnserialise( InputArchive& arch );


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

		//! Base image to resize.
		NodeImagePtr GetBase() const;
		void SetBase( NodeImagePtr );

		//! Is the size a relative factor or an absolute size?
		void SetRelative( bool );

		//! New size or relative factor
		void SetSize( float x, float y );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageResize();

	private:

		Private* m_pD;

	};


}
