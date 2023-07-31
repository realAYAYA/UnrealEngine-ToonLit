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

    class NodeColour;
    typedef Ptr<NodeColour> NodeColourPtr;
    typedef Ptr<const NodeColour> NodeColourPtrConst;

    class NodeScalar;
    typedef Ptr<NodeScalar> NodeScalarPtr;
    typedef Ptr<const NodeScalar> NodeScalarPtrConst;

    class NodeString;
    typedef Ptr<NodeString> NodeStringPtr;
    typedef Ptr<const NodeString> NodeStringPtrConst;

    class NodeSurface;
    typedef Ptr<NodeSurface> NodeSurfacePtr;
    typedef Ptr<const NodeSurface> NodeSurfacePtrConst;

    class NodeSurfaceNew;
    typedef Ptr<NodeSurfaceNew> NodeSurfaceNewPtr;
    typedef Ptr<const NodeSurfaceNew> NodeSurfaceNewPtrConst;



    //! This node makes a new Surface from several meshes and images.
	//! \ingroup model
    class MUTABLETOOLS_API NodeSurfaceNew : public NodeSurface
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeSurfaceNew();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeSurfaceNew* pNode, OutputArchive& arch );
        static NodeSurfaceNewPtr StaticUnserialise( InputArchive& arch );

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

        //! Get the name of the Surface.
		const char* GetName() const;

        //! Set the name of the Surface.
        void SetName( const char* );

        //! Add an optional, opaque id that will be returned in the surfaces of the created
        //! instances. Can be useful to identify surfaces on the application side.
        //! See Instance::GetSurfaceCustomId
        void SetCustomID( uint32 id );

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

		//! \name Meshes
        //! \{

        //! Get the number of meshes in the Surface.
		int GetMeshCount() const;

        //! Set the number of meshes in the Surface.
		void SetMeshCount( int );

        //! Get the node generating one of the meshes in the Surface.
		//! \param index index of the mesh, from 0 to GetMeshCount()-1
		NodeMeshPtr GetMesh( int index ) const;

        //! Set the node generating one of the meshes in the Surface.
		//! \param index index of the mesh, from 0 to GetMeshCount()-1
		void SetMesh( int index, NodeMeshPtr );

        //! Get the name of a mesh in the Surface.
		//! \param index index of the mesh, from 0 to GetMeshCount()-1
		const char* GetMeshName( int index ) const;

        //! Set the name of a mesh in the Surface.
		//! \param index index of the mesh, from 0 to GetMeshCount()-1
		//! \param strName name of the mesh
		void SetMeshName( int index, const char* strName );

		//! \}


		//! \name Images
		//! \{

		//! Get the number of images in the Surface.
		int GetImageCount() const;

		//! Set the number of images in the Surface.
		void SetImageCount(int);

		//! Get the node generating one of the images in the Surface.
		//! \param index index of the image, from 0 to GetImageCount()-1
		NodeImagePtr GetImage(int index) const;

		//! Set the node generating one of the images in the Surface.
		//! \param index index of the image, from 0 to GetImageCount()-1
		void SetImage(int index, NodeImagePtr);

		//! Get the name of a image in the Surface.
		//! \param index index of the image, from 0 to GetImageCount()-1
		const char* GetImageName(int index) const;

        //! Set the name of a image in the Surface.
        //! \param index index of the image, from 0 to GetImageCount()-1
        //! \param strName name of the image
        void SetImageName( int index, const char* strName );

        //! Get the name of a image in the Surface.
        //! \param index index of the image, from 0 to GetImageCount()-1
        int GetImageLayoutIndex( int index ) const;

        //! Set the name of a image in the Surface.
        //! \param index index of the image, from 0 to GetImageCount()-1
        //! \param layoutIndex index of the layout that will be used with this image.
        void SetImageLayoutIndex( int imageIndex, int layoutIndex );

		//! This can be used to ser additional information for error reporting.
		//! It is not propagated to compiled objects in any way.
		void SetImageAdditionalNames(int index, const char* strMaterialName, const char* strMaterialParameterName);

        //! \}


        //! \name Vectors
        //! \{

        //! Get the number of vectors in the Surface.
        int GetVectorCount() const;

        //! Set the number of vectors in the Surface.
        void SetVectorCount(int);

        //! Get the node generating one of the vectors in the Surface.
        //! \param index index of the vector, from 0 to GetVectorCount()-1
        NodeColourPtr GetVector(int index) const;

        //! Set the node generating one of the vectors in the Surface.
        //! \param index index of the vector, from 0 to GetVectorCount()-1
        void SetVector(int index, NodeColourPtr);

        //! Get the name of a vectors in the Surface.
        //! \param index index of the vector, from 0 to GetVectorCount()-1
        const char* GetVectorName(int index) const;

        //! Set the name of a image in the Surface.
        //! \param index index of the vector, from 0 to GetVectorCount()-1
        //! \param strName name of the vector
        void SetVectorName(int index, const char* strName);

        //! \}


        //! \name Scalars
        //! \{

        //! Get the number of Scalars in the Surface.
        int GetScalarCount() const;

        //! Set the number of Scalars in the Surface.
        void SetScalarCount( int );

        //! Get the node generating one of the Scalars in the Surface.
        //! \param index index of the Scalar, from 0 to GetScalarCount()-1
        NodeScalarPtr GetScalar( int index ) const;

        //! Set the node generating one of the Scalars in the Surface.
        //! \param index index of the Scalar, from 0 to GetScalarCount()-1
        void SetScalar( int index, NodeScalarPtr );

        //! Get the name of a Scalars in the Surface.
        //! \param index index of the Scalar, from 0 to GetScalarCount()-1
        const char* GetScalarName( int index ) const;

        //! Set the name of a image in the Surface.
        //! \param index index of the Scalar, from 0 to GetScalarCount()-1
        //! \param strName name of the Scalar
        void SetScalarName( int index, const char* strName );

        //! \}


        //! \name Strings
        //! \{

        //! Get the number of strings in the Surface.
        int GetStringCount() const;

        //! Set the number of strings in the Surface.
        void SetStringCount( int );

        //! Get the node generating one of the strings in the Surface.
        //! \param index index of the strings, from 0 to GetStringCount()-1
        NodeStringPtr GetString( int index ) const;

        //! Set the node generating one of the Strings in the Surface.
        //! \param index index of the String, from 0 to GetStringCount()-1
        void SetString( int index, NodeStringPtr );

        //! Get the name of a String in the Surface.
        //! \param index index of the String, from 0 to GetStringCount()-1
        const char* GetStringName( int index ) const;

        //! Set the name of a image in the Surface.
        //! \param index index of the String, from 0 to GetStringCount()-1
        //! \param strName name of the String
        void SetStringName( int index, const char* strName );

        //! \}

        //-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeSurfaceNew();

	private:

		Private* m_pD;

	};


}
