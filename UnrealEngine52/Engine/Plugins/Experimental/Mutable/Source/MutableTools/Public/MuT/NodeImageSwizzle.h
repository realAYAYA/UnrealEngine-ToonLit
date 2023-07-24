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
	class NodeImageSwizzle;
	typedef Ptr<NodeImageSwizzle> NodeImageSwizzlePtr;
	typedef Ptr<const NodeImageSwizzle> NodeImageSwizzlePtrConst;

	class InputArchive;
	class OutputArchive;


	//! Node that composes a new image by gathering pixel data from channels in other images.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageSwizzle : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageSwizzle();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageSwizzle* pNode, OutputArchive& arch );
		static NodeImageSwizzlePtr StaticUnserialise( InputArchive& arch );


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

		//!
		EImageFormat GetFormat() const;
		void SetFormat(EImageFormat);

		//!
		NodeImagePtr GetSource( int ) const;
		int GetSourceChannel( int ) const;

		void SetSource( int, NodeImagePtr );
		void SetSourceChannel( int, int );


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageSwizzle();

	private:

		Private* m_pD;

	};


}
