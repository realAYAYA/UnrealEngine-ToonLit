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
	class NodeImageFormat;
	typedef Ptr<NodeImageFormat> NodeImageFormatPtr;
	typedef Ptr<const NodeImageFormat> NodeImageFormatPtrConst;

	class InputArchive;
	class OutputArchive;


	//! Node that composes a new image by gathering pixel data from channels in other images.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageFormat : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageFormat();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageFormat* pNode, OutputArchive& arch );
		static NodeImageFormatPtr StaticUnserialise( InputArchive& arch );


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

		//!
		EImageFormat GetFormat() const;
        void SetFormat(EImageFormat format, EImageFormat formatIfAlpha = EImageFormat::IF_NONE );

		//!
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
		~NodeImageFormat();

	private:

		Private* m_pD;

	};


}

