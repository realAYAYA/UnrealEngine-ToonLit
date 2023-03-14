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

    class NodeImageMipmap;
    typedef Ptr<NodeImageMipmap> NodeImageMipmapPtr;
    typedef Ptr<const NodeImageMipmap> NodeImageMipmapPtrConst;


	//! Generate mimaps for an image.
	//! \ingroup model
    class MUTABLETOOLS_API NodeImageMipmap : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeImageMipmap();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageMipmap* pNode, OutputArchive& arch );
        static NodeImageMipmapPtr StaticUnserialise( InputArchive& arch );


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

        //! Get the node generating the source image to be mipmapped.
		NodeImagePtr GetSource() const;
		void SetSource( NodeImagePtr );

		void SetMipmapGenerationSettings( EMipmapFilterType filterType,
										  EAddressMode addressMode,
										  float sharpenFactor,  
										  bool bDitherMipAlpha );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeImageMipmap();

	private:

		Private* m_pD;

	};


}
