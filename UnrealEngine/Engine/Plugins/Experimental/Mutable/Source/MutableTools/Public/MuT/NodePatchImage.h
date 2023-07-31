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
    class NodeImage;
    typedef Ptr<NodeImage> NodeImagePtr;
    typedef Ptr<const NodeImage> NodeImagePtrConst;

    class NodePatchImage;
    typedef Ptr<NodePatchImage> NodePatchImagePtr;
    typedef Ptr<const NodePatchImage> NodePatchImagePtrConst;


    //! Node that allows to modify an image from an object by blending other images on specific
    //! layout blocks.
    //! \ingroup model
    class MUTABLETOOLS_API NodePatchImage : public Node
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        NodePatchImage();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodePatchImage* pNode, OutputArchive& arch );
        static NodePatchImagePtr StaticUnserialise( InputArchive& arch );


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

        //! Set the image that will be blended on the destination.
        void SetImage( NodeImagePtr );

        //! Get the image that will be blended on the destination.
        NodeImagePtr GetImage() const;

        //! Set the blending mask image.
        void SetMask( NodeImagePtr );

        //! Get the blending mask image.
        NodeImagePtr GetMask() const;

        //! Set the number of blocks in the layout that will be patched
        void SetBlockCount( int );

        //! Get the number of blocks in the layout that will be patched
        int GetBlockCount() const;

        //! Set a block of the layout that will be patched.
        //! \param index is the index in the patching list, from 0 to GetBlockCount()-1
        //! \param block is the block index in the layout of the image were this patch operation
        //! will be applied.
        void SetBlock( int index, int block );

        //! Get a block of the layout that will be patched.
        //! \param index is the index in the patching list, from 0 to GetBlockCount()-1
        int GetBlock( int index ) const;

        //! Set the blending operation to use to combine the pixels
        EBlendType GetBlendType() const;
        void SetBlendType(EBlendType);

        //! Enable patching the alpha channel if present
        bool GetApplyToAlphaChannel() const;
        void SetApplyToAlphaChannel( bool );


        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;
        Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

    protected:

        //! Forbidden. Manage with the Ptr<> template.
        ~NodePatchImage();

    private:

        Private* m_pD;

    };



}
