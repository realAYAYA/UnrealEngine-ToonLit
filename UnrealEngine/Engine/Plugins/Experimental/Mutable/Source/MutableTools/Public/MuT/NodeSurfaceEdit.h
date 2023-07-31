// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeSurface.h"


namespace mu
{

	// Forward definitions
    class NodeMesh;
    typedef Ptr<NodeMesh> NodeMeshPtr;
    typedef Ptr<const NodeMesh> NodeMeshPtrConst;

    class NodeImage;
	typedef Ptr<NodeImage> NodeImagePtr;
	typedef Ptr<const NodeImage> NodeImagePtrConst;

	class NodePatchMesh;
	typedef Ptr<NodePatchMesh> NodePatchMeshPtr;
	typedef Ptr<const NodePatchMesh> NodePatchMeshPtrConst;

	class NodePatchImage;
	typedef Ptr<NodePatchImage> NodePatchImagePtr;
	typedef Ptr<const NodePatchImage> NodePatchImagePtrConst;

    class NodeSurfaceEdit;
    typedef Ptr<NodeSurfaceEdit> NodeSurfaceEditPtr;
    typedef Ptr<const NodeSurfaceEdit> NodeSurfaceEditPtrConst;

	class NodeScalar;
	typedef Ptr<NodeScalar> NodeScalarPtr;
	typedef Ptr<const NodeScalar> NodeScalarPtrConst;



	//! This node modifies a node of the parent object of the object that this node belongs to.
    //! It allows to extend, cut and morph the parent Surface's meshes.
    //! It also allows to patch the parent Surface's textures.
	//! \ingroup model
    class MUTABLETOOLS_API NodeSurfaceEdit : public NodeSurface
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeSurfaceEdit();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeSurfaceEdit* pNode, OutputArchive& arch );
        static NodeSurfaceEditPtr StaticUnserialise( InputArchive& arch );

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

        //! Set the parent Surface to modify. It should be a node from the same LOD of one of
		//! the parent objects of this node's object.
        void SetParent( NodeSurface* );
        NodeSurface* GetParent() const;

        //! \name Tags
        //! \{

        //! Add a tag to the surface:
        //! - the surface will be affected by modifier nodes with the same tag
        //! - the tag will be enabled when the surface is added to an object, and it can activate
        //! variations for any surface.
        void AddTag(const char* tagName);

        //! Get the number of tags added to the Surface.
        int GetTagCount() const;

        //! Get a tag string from an index (0 to GetTagCount-1)
        const char* GetTag( int ) const;

        //! \}

		//! \name Mesh modification
		//! \{

		//! Mesh a node to extend the parent mesh
        void SetMesh( NodePatchMesh* );

		//! Get the node to extend the parent mesh
        NodePatchMesh* GetMesh() const;

		//! Set a node to morph the parent mesh
        void SetMorph( NodeMeshPtr );

		//! Get the node to morph the parent mesh
        NodeMeshPtr GetMorph() const;

		//! Set the factor: weight used to select (and combine) the morphs to apply.
		void SetFactor( NodeScalarPtr );

		//! Get the factor: weight used to select (and combine) the morphs to apply.
		NodeScalarPtr GetFactor() const;

		//! \}

		//! \name Image modification
		//! \{

		//! Get the number of images to modify. Should match the parent image count.
		int GetImageCount() const;

		//! Set the number of images to modify. Should match the parent image count.
		void SetImageCount( int );

		//! Get the image node that extends a parent image, adding new blocks
		NodeImagePtr GetImage( int index ) const;

		//! Set the image node that extends a parent image, adding new blocks
		void SetImage( int index, NodeImagePtr );

		//! Get the image node that patches a parent image, blending on existing blocks
		NodePatchImagePtr GetPatch( int index ) const;

		//! Set the image node that patches a parent image, blending on existing blocks
		void SetPatch( int index, NodePatchImagePtr );

		//!}


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeSurfaceEdit();

	private:

		Private* m_pD;

	};



}

